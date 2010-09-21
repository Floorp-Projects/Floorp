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

// This module contains the platform-specific code. This make the rest of the
// code less dependent on operating system, compilers and runtime libraries.
// This module does specifically not deal with differences between different
// processor architecture.
// The platform classes have the same definition for all platforms. The
// implementation for a particular platform is put in platform_<os>.cc.
// The build system then uses the implementation for the target platform.
//
// This design has been chosen because it is simple and fast. Alternatively,
// the platform dependent classes could have been implemented using abstract
// superclasses with virtual methods and having specializations for each
// platform. This design was rejected because it was more complicated and
// slower. It would require factory methods for selecting the right
// implementation and the overhead of virtual methods for performance
// sensitive like mutex locking/unlocking.

#ifndef V8_PLATFORM_H_
#define V8_PLATFORM_H_

// Windows specific stuff.
#ifdef WIN32

// Microsoft Visual C++ specific stuff.
#ifdef _MSC_VER

enum {
  FP_NAN,
  FP_INFINITE,
  FP_ZERO,
  FP_SUBNORMAL,
  FP_NORMAL
};

int fpclassify(double x);

int strncasecmp(const char* s1, const char* s2, int n);

#endif  // _MSC_VER

#endif  // WIN32


// GCC specific stuff
#ifdef __GNUC__

// Needed for va_list on at least MinGW and Android.
#include <stdarg.h>

#define __GNUC_VERSION__ (__GNUC__ * 10000 + __GNUC_MINOR__ * 100)

#endif  // __GNUC__

namespace v8 {
namespace internal {

double ceiling(double x);

// ----------------------------------------------------------------------------
// OS
//
// This class has static methods for the different platform specific
// functions. Add methods here to cope with differences between the
// supported platforms.

class OS {
 public:
  // Abort the current process.
  static void Abort();
};

} }  // namespace v8::internal

#endif  // V8_PLATFORM_H_
