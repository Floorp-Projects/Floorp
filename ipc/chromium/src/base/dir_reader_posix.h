/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_DIR_READER_POSIX_H_
#define BASE_DIR_READER_POSIX_H_
#pragma once

#include "build/build_config.h"

// This header provides a class, DirReaderPosix, which allows one to open and
// read from directories without allocating memory. For the interface, see
// the generic fallback in dir_reader_fallback.h.

// Mac note: OS X has getdirentries, but it only works if we restrict Chrome to
// 32-bit inodes. There is a getdirentries64 syscall in 10.6, but it's not
// wrapped and the direct syscall interface is unstable. Using an unstable API
// seems worse than falling back to enumerating all file descriptors so we will
// probably never implement this on the Mac.

#if defined(OS_LINUX)
#include "base/dir_reader_linux.h"
#elif defined(OS_BSD) && !defined(__GLIBC__)
#include "base/dir_reader_bsd.h"
#else
#include "base/dir_reader_fallback.h"
#endif

namespace base {

#if defined(OS_LINUX)
typedef DirReaderLinux DirReaderPosix;
#elif defined(OS_BSD) && !defined(__GLIBC__)
typedef DirReaderBSD DirReaderPosix;
#else
typedef DirReaderFallback DirReaderPosix;
#endif

}  // namespace base

#endif // BASE_DIR_READER_POSIX_H_
