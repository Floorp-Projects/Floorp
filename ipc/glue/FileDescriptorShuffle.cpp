/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileDescriptorShuffle.h"

#include "base/eintr_wrapper.h"
#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"

#include <algorithm>
#include <unistd.h>
#include <fcntl.h>

namespace mozilla {
namespace ipc {

// F_DUPFD_CLOEXEC is like F_DUPFD (see below) but atomically makes
// the new fd close-on-exec.  This is useful to prevent accidental fd
// leaks into processes created by plain fork/exec, but IPC uses
// CloseSuperfluousFds so it's not essential.
//
// F_DUPFD_CLOEXEC is in POSIX 2008, but as of 2018 there are still
// some OSes that don't support it.  (Specifically: Solaris 10 doesn't
// have it, and Android should have kernel support but doesn't define
// the constant until API 21 (Lollipop).  We also don't use this for
// IPC child launching on Android, so there's no point in hard-coding
// the definitions like we do for Android in some other cases.)
#ifdef F_DUPFD_CLOEXEC
static const int kDupFdCmd = F_DUPFD_CLOEXEC;
#else
static const int kDupFdCmd = F_DUPFD;
#endif

// This implementation ensures that the *ranges* of the source and
// destination fds don't overlap, which is unnecessary but sufficient
// to avoid conflicts or identity mappings.
//
// In practice, the source fds will usually be large and the
// destination fds will all be relatively small, so there mostly won't
// be temporary fds.  This approach has the advantage of being simple
// (and linear-time, but hopefully there aren't enough fds for that to
// matter).
bool
FileDescriptorShuffle::Init(MappingRef aMapping)
{
  MOZ_ASSERT(mMapping.IsEmpty());

  // Find the maximum destination fd; any source fds not greater than
  // this will be duplicated.
  int maxDst = STDERR_FILENO;
  for (const auto& elem : aMapping) {
    maxDst = std::max(maxDst, elem.second);
  }
  mMaxDst = maxDst;

#ifdef DEBUG
  // Increase the limit to make sure the F_DUPFD case gets test coverage.
  if (!aMapping.IsEmpty()) {
    // Try to find a value that will create a nontrivial partition.
    int fd0 = aMapping[0].first;
    int fdn = aMapping.rbegin()->first;
    maxDst = std::max(maxDst, (fd0 + fdn) / 2);
  }
#endif

  for (const auto& elem : aMapping) {
    int src = elem.first;
    // F_DUPFD is like dup() but allows placing a lower bound
    // on the new fd, which is exactly what we want.
    if (src <= maxDst) {
      src = fcntl(src, kDupFdCmd, maxDst + 1);
      if (src < 0) {
        return false;
      }
      mTempFds.AppendElement(src);
    }
    MOZ_ASSERT(src > maxDst);
#ifdef DEBUG
    // Check for accidentally mapping two different fds to the same
    // destination.  (This is O(n^2) time, but it shouldn't matter.)
    for (const auto& otherElem : mMapping) {
      MOZ_ASSERT(elem.second != otherElem.second);
    }
#endif
    mMapping.AppendElement(std::make_pair(src, elem.second));
  }
  return true;
}

bool
FileDescriptorShuffle::MapsTo(int aFd) const
{
  // Prune fds that are too large to be a destination, rather than
  // searching; this should be the common case.
  if (aFd > mMaxDst) {
    return false;
  }
  for (const auto& elem : mMapping) {
    if (elem.second == aFd) {
      return true;
    }
  }
  return false;
}

FileDescriptorShuffle::~FileDescriptorShuffle()
{
  for (const auto& fd : mTempFds) {
    mozilla::DebugOnly<int> rv = IGNORE_EINTR(close(fd));
    MOZ_ASSERT(rv == 0);
  }
}

} // namespace ipc
} // namespace mozilla
