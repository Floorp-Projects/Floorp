// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_descriptor_shuffle.h"

#include <errno.h>
#include <unistd.h>

#include "base/eintr_wrapper.h"
#include "base/logging.h"

namespace base {

bool PerformInjectiveMultimap(const InjectiveMultimap& m_in,
                              InjectionDelegate* delegate) {
  InjectiveMultimap m(m_in);
  std::vector<int> extra_fds;

  for (InjectiveMultimap::iterator i = m.begin(); i != m.end(); ++i) {
    int temp_fd = -1;

    // We DCHECK the injectiveness of the mapping.
    for (InjectiveMultimap::iterator j = i + 1; j != m.end(); ++j)
      DCHECK(i->dest != j->dest);

    const bool is_identity = i->source == i->dest;

    for (InjectiveMultimap::iterator j = i + 1; j != m.end(); ++j) {
      if (!is_identity && i->dest == j->source) {
        if (temp_fd == -1) {
          if (!delegate->Duplicate(&temp_fd, i->dest))
            return false;
          extra_fds.push_back(temp_fd);
        }

        j->source = temp_fd;
        j->close = false;
      }

      if (i->close && i->source == j->dest)
        i->close = false;

      if (i->close && i->source == j->source) {
        i->close = false;
        j->close = true;
      }
    }

    if (!is_identity) {
      if (!delegate->Move(i->source, i->dest))
        return false;
    }

    if (!is_identity && i->close)
      delegate->Close(i->source);
  }

  for (std::vector<int>::const_iterator
       i = extra_fds.begin(); i != extra_fds.end(); ++i) {
    delegate->Close(*i);
  }

  return true;
}

bool FileDescriptorTableInjection::Duplicate(int* result, int fd) {
  *result = HANDLE_EINTR(dup(fd));
  return *result >= 0;
}

bool FileDescriptorTableInjection::Move(int src, int dest) {
  return HANDLE_EINTR(dup2(src, dest)) != -1;
}

void FileDescriptorTableInjection::Close(int fd) {
  HANDLE_EINTR(close(fd));
}

}  // namespace base
