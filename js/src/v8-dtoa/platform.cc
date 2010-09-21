// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// MOZ: This file is a merge of the relevant parts of all the platform-*.cc
// files in v8;  the amount of code remaining was small enough that putting
// everything in a single file was easier, particularly because it means we
// can always compile this one file on every platform.

#include <math.h>
#include <signal.h>
#include <stdlib.h>

#include "v8.h"

namespace v8 {
namespace internal {

double ceiling(double x) {
#if defined(__APPLE__)
  // Correct Mac OS X Leopard 'ceil' behavior.
  //
  // MOZ: This appears to be fixed in Mac OS X 10.5.8.
  //
  // MOZ: This fix is apprently also required for FreeBSD and OpenBSD, if we
  // have to worry about them.
  if (-1.0 < x && x < 0.0) {
    return -0.0;
  } else {
    return ceil(x);
  }
#else
  return ceil(x);
#endif
}


// MOZ: These exit behaviours were copied from SpiderMonkey's JS_Assert()
// function.
void OS::Abort() {
#if defined(WIN32)
    /*
     * We used to call DebugBreak() on Windows, but amazingly, it causes
     * the MSVS 2010 debugger not to be able to recover a call stack.
     */
    *((int *) NULL) = 0;
    exit(3);
#elif defined(__APPLE__)
    /*
     * On Mac OS X, Breakpad ignores signals. Only real Mach exceptions are
     * trapped.
     */
    *((int *) NULL) = 0;  /* To continue from here in GDB: "return" then "continue". */
    raise(SIGABRT);  /* In case above statement gets nixed by the optimizer. */
#else
    raise(SIGABRT);  /* To continue from here in GDB: "signal 0". */
#endif
}

} }  // namespace v8::internal

// Extra POSIX/ANSI routines for Win32 when when using Visual Studio C++. Please
// refer to The Open Group Base Specification for specification of the correct
// semantics for these functions.
// (http://www.opengroup.org/onlinepubs/000095399/)
#ifdef _MSC_VER

#include <float.h>

// Classify floating point number - usually defined in math.h
int fpclassify(double x) {
  // Use the MS-specific _fpclass() for classification.
  int flags = _fpclass(x);

  // Determine class. We cannot use a switch statement because
  // the _FPCLASS_ constants are defined as flags.
  if (flags & (_FPCLASS_PN | _FPCLASS_NN)) return FP_NORMAL;
  if (flags & (_FPCLASS_PZ | _FPCLASS_NZ)) return FP_ZERO;
  if (flags & (_FPCLASS_PD | _FPCLASS_ND)) return FP_SUBNORMAL;
  if (flags & (_FPCLASS_PINF | _FPCLASS_NINF)) return FP_INFINITE;

  // All cases should be covered by the code above.
  ASSERT(flags & (_FPCLASS_SNAN | _FPCLASS_QNAN));
  return FP_NAN;
}


#endif  // _MSC_VER
