/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_COMPILER_SPECIFIC_H_
#define BASE_COMPILER_SPECIFIC_H_

#include "build/build_config.h"

#if defined(COMPILER_MSVC)

// Macros for suppressing and disabling warnings on MSVC.
//
// Warning numbers are enumerated at:
// http://msdn.microsoft.com/en-us/library/8x5x43k7(VS.80).aspx
//
// The warning pragma:
// http://msdn.microsoft.com/en-us/library/2c8f766e(VS.80).aspx
//
// Using __pragma instead of #pragma inside macros:
// http://msdn.microsoft.com/en-us/library/d9x1s805.aspx

// MSVC_SUPPRESS_WARNING disables warning |n| for the remainder of the line and
// for the next line of the source file.
#define MSVC_SUPPRESS_WARNING(n) __pragma(warning(suppress:n))

// MSVC_PUSH_DISABLE_WARNING pushes |n| onto a stack of warnings to be disabled.
// The warning remains disabled until popped by MSVC_POP_WARNING.
#define MSVC_PUSH_DISABLE_WARNING(n) __pragma(warning(push)) \
                                     __pragma(warning(disable:n))

// MSVC_PUSH_WARNING_LEVEL pushes |n| as the global warning level.  The level
// remains in effect until popped by MSVC_POP_WARNING().  Use 0 to disable all
// warnings.
#define MSVC_PUSH_WARNING_LEVEL(n) __pragma(warning(push, n))

// Pop effects of innermost MSVC_PUSH_* macro.
#define MSVC_POP_WARNING() __pragma(warning(pop))

#define MSVC_DISABLE_OPTIMIZE() __pragma(optimize("", off))
#define MSVC_ENABLE_OPTIMIZE() __pragma(optimize("", on))

// Allows |this| to be passed as an argument in constructor initializer lists.
// This uses push/pop instead of the seemingly simpler suppress feature to avoid
// having the warning be disabled for more than just |code|.
//
// Example usage:
// Foo::Foo() : x(NULL), ALLOW_THIS_IN_INITIALIZER_LIST(y(this)), z(3) {}
//
// Compiler warning C4355: 'this': used in base member initializer list:
// http://msdn.microsoft.com/en-us/library/3c594ae3(VS.80).aspx
#define ALLOW_THIS_IN_INITIALIZER_LIST(code) MSVC_PUSH_DISABLE_WARNING(4355) \
                                             code \
                                             MSVC_POP_WARNING()

#else  // Not MSVC

#define MSVC_SUPPRESS_WARNING(n)
#define MSVC_PUSH_DISABLE_WARNING(n)
#define MSVC_PUSH_WARNING_LEVEL(n)
#define MSVC_POP_WARNING()
#define MSVC_DISABLE_OPTIMIZE()
#define MSVC_ENABLE_OPTIMIZE()
#define ALLOW_THIS_IN_INITIALIZER_LIST(code) code

#endif  // COMPILER_MSVC


#if defined(COMPILER_GCC)
#define ALLOW_UNUSED __attribute__((unused))
#define WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else  // Not GCC
#define ALLOW_UNUSED
#define WARN_UNUSED_RESULT
#endif

#endif  // BASE_COMPILER_SPECIFIC_H_
