/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PROCESS_H_
#define BASE_PROCESS_H_

#include "base/basictypes.h"

#include <sys/types.h>
#ifdef XP_WIN
#  include <windows.h>
#endif

namespace base {

// ProcessHandle is a platform specific type which represents the underlying OS
// handle to a process.
// ProcessId is a number which identifies the process in the OS.
#if defined(XP_WIN)
typedef HANDLE ProcessHandle;
typedef DWORD ProcessId;
// inttypes.h-like macro for ProcessId formatting.
#  define PRIPID "lu"

const ProcessHandle kInvalidProcessHandle = INVALID_HANDLE_VALUE;

// In theory, on Windows, this is a valid process ID, but in practice they are
// currently divisible by four. Process IDs share the kernel handle allocation
// code and they are guaranteed to be divisible by four.
// As this could change for process IDs we shouldn't generally rely on this
// property, however even if that were to change, it seems safe to rely on this
// particular value never being used.
const ProcessId kInvalidProcessId = kuint32max;
#else
// On POSIX, our ProcessHandle will just be the PID.
typedef pid_t ProcessHandle;
typedef pid_t ProcessId;
// inttypes.h-like macro for ProcessId formatting.
#  define PRIPID "d"

const ProcessHandle kInvalidProcessHandle = -1;
const ProcessId kInvalidProcessId = -1;
#endif

}  // namespace base

#endif  // BASE_PROCESS_H_
