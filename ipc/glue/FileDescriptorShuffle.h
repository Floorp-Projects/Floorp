/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_FileDescriptorShuffle_h
#define mozilla_ipc_FileDescriptorShuffle_h

#include "mozilla/Span.h"
#include "nsTArray.h"

#include <functional>
#include <utility>

// This class converts a set of file descriptor mapping, which may
// contain conflicts (like {a->b, b->c} or {a->b, b->a}) into a
// sequence of dup2() operations that can be performed between fork
// and exec, or with posix_spawn_file_actions_adddup2.  It may create
// temporary duplicates of fds to use as the source of a dup2; they
// are closed on destruction.
//
// The dup2 sequence is guaranteed to not contain dup2(x, x) for any
// x; if such an element is present in the input, it will be dup2()ed
// from a temporary fd to ensure that the close-on-exec bit is cleared.
//
// In general, this is *not* guaranteed to minimize the use of
// temporary fds.

namespace mozilla {
namespace ipc {

class FileDescriptorShuffle
{
public:
  FileDescriptorShuffle() = default;
  ~FileDescriptorShuffle();

  using MappingRef = mozilla::Span<const std::pair<int, int>>;

  // Translate the given mapping, creating temporary fds as needed.
  // Can fail (return false) on failure to duplicate fds.
  bool Init(MappingRef aMapping);

  // Accessor for the dup2() sequence.  Do not use the returned value
  // or the fds contained in it after this object is destroyed.
  MappingRef Dup2Sequence() const { return mMapping; }

  // Tests whether the given fd is used as a destination in this mapping.
  // Can be used to close other fds after performing the dup2()s.
  bool MapsTo(int aFd) const;

  // Wraps MapsTo in a function object, as a convenience for use with
  // base::CloseSuperfluousFds.
  std::function<bool(int)> MapsToFunc() const {
    return [this](int fd) { return MapsTo(fd); };
  }

private:
  nsTArray<std::pair<int, int>> mMapping;
  nsTArray<int> mTempFds;
  int mMaxDst;

  FileDescriptorShuffle(const FileDescriptorShuffle&) = delete;
  void operator=(const FileDescriptorShuffle&) = delete;
};

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_FileDescriptorShuffle_h
