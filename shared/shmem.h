/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Provides an abstraction for constructing shared memory mappings between two
// (or more) processes.  Mappings are huge-page backed, with synchronization for
// versioning, and client initialization.
//
// Currently, a process can host an arbitrary number of shmem regions, but they
// must each have a unique name.  There is no limit on how many clients may
// connect to a processes region.
//
// Connecting clients must have the ability to examine open file descriptors of
// the remote process.  Generally speaking, for the ghost use-case, this is not
// a particular impingement as we expect processes to host shared memory with
// their scheduling requirements and privileged agents to be the connecting
// clients.
#ifndef GHOST_SHARED_SHMEM_H
#define GHOST_SHARED_SHMEM_H

#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdint>

#include "lib/base.h"

namespace ghost {

class GhostShmem {
 public:
  GhostShmem() {}
  // Constructs a new named shared memory region hosted by the current process.
  // It is guaranteed that the useful size will be at least "size".
  // REQUIRES: "name" must uniquely identify this region.
  GhostShmem(int64_t client_version, const char* name, size_t size);
  ~GhostShmem();

  // Connects to the region identified by "name", hosted by the process "pid".
  // REQUIRES: "pid" hosting "name" must exist.
  bool Attach(int64_t client_version, const char* name, pid_t pid);

  // Called by clients when they are aready for remote connections to proceed.
  // REQUIRES: Must be called.
  void MarkReady();

  // A raw byte mapping into the hosted shared memory region.
  inline char* bytes() { return static_cast<char*>(data_); }

  // This is the client usable bytes addressable via bytes().  It will be at
  // least as large as requested at time of construction.
  size_t size();

  // This includes internal overheads and roundings on the mapping.
  size_t absolute_size() const { return map_size_; }
  inline const void* absolute_start() const { return shmem_; }

  // Internal overheads that clients may optimized passed mapping sizes against.
  // This is useful as it represents the padding that should be considered if
  // trying to optimally pack against the huge-page backing.
  static size_t OverHeadbytes() { return kHeaderReservedBytes; }

  GhostShmem(const GhostShmem&) = delete;
  GhostShmem(GhostShmem&&) = delete;

  static GhostShmem* GetShmemBlob(size_t size);

 private:
  struct InternalHeader;

  void WaitForReady();

  static int memfd_create(const char* name, unsigned int flags) {
    return syscall(__NR_memfd_create, name, flags);
  }
  void CreateShmem(int64_t client_version, const char* suffix, size_t size);
  bool ConnectShmem(int64_t client_version, const char* suffix, pid_t pid);

  // These members describe the shared memory area.
  void* shmem_ = nullptr;
  size_t map_size_;
  int memfd_ = -1;
  // These members map into the shared memory area.
  InternalHeader* hdr_ = nullptr;
  void* data_;

  static int OpenGhostShmemFd(const char* suffix, pid_t pid);
  static constexpr int kHeaderReservedBytes = 4096;  // PAGE_SIZE
};

}  // namespace ghost

#endif  // GHOST_SHARED_SHMEM_H
