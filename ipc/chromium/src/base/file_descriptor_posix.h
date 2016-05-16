/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILE_DESCRIPTOR_POSIX_H_
#define BASE_FILE_DESCRIPTOR_POSIX_H_

namespace base {

// -----------------------------------------------------------------------------
// We introduct a special structure for file descriptors in order that we are
// able to use template specialisation to special-case their handling.
//
// WARNING: (Chromium only) There are subtleties to consider if serialising
// these objects over IPC. See comments in chrome/common/ipc_message_utils.h
// above the template specialisation for this structure.
// -----------------------------------------------------------------------------
struct FileDescriptor {
  FileDescriptor()
      : fd(-1),
        auto_close(false) { }

  FileDescriptor(int ifd, bool iauto_close)
      : fd(ifd),
        auto_close(iauto_close) { }

  int fd;
  // If true, this file descriptor should be closed after it has been used. For
  // example an IPC system might interpret this flag as indicating that the
  // file descriptor it has been given should be closed after use.
  bool auto_close;
};

}  // namespace base

#endif  // BASE_FILE_DESCRIPTOR_POSIX_H_
