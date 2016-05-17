/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This provides a wrapper around system calls which may be interrupted by a
// signal and return EINTR. See man 7 signal.
//
// On Windows, this wrapper macro does nothing.

#ifndef BASE_EINTR_WRAPPER_H_
#define BASE_EINTR_WRAPPER_H_

#include "build/build_config.h"

#if defined(OS_POSIX)

#include <errno.h>

#define HANDLE_EINTR(x) ({ \
  typeof(x) __eintr_result__; \
  do { \
    __eintr_result__ = x; \
  } while (__eintr_result__ == -1 && errno == EINTR); \
  __eintr_result__;\
})

#else

#define HANDLE_EINTR(x) x

#endif  // OS_POSIX

#endif  // !BASE_EINTR_WRAPPER_H_
