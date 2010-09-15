// Copyright 2006-2009 the V8 project authors. All rights reserved.
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

#ifndef V8_GLOBALS_H_
#define V8_GLOBALS_H_

namespace v8 {
namespace internal {

// Processor architecture detection.  For more info on what's defined, see:
//   http://msdn.microsoft.com/en-us/library/b0084kay.aspx
//   http://www.agner.org/optimize/calling_conventions.pdf
//   or with gcc, run: "echo | gcc -E -dM -"
#if defined(_M_X64) || defined(__x86_64__)
#define V8_HOST_ARCH_X64 1
#define V8_HOST_ARCH_64_BIT 1
#define V8_HOST_CAN_READ_UNALIGNED 1
#elif defined(_M_IX86) || defined(__i386__)
#define V8_HOST_ARCH_IA32 1
#define V8_HOST_ARCH_32_BIT 1
#define V8_HOST_CAN_READ_UNALIGNED 1
#elif defined(__ARMEL__)
#define V8_HOST_ARCH_ARM 1
#define V8_HOST_ARCH_32_BIT 1
// Some CPU-OS combinations allow unaligned access on ARM. We assume
// that unaligned accesses are not allowed unless the build system
// defines the CAN_USE_UNALIGNED_ACCESSES macro to be non-zero.
#if CAN_USE_UNALIGNED_ACCESSES
#define V8_HOST_CAN_READ_UNALIGNED 1
#endif
#elif defined(_MIPS_ARCH_MIPS32R2)
#define V8_HOST_ARCH_MIPS 1
#define V8_HOST_ARCH_32_BIT 1
#else
#error Host architecture was not detected as supported by v8
#endif

// Target architecture detection. This may be set externally. If not, detect
// in the same way as the host architecture, that is, target the native
// environment as presented by the compiler.
#if !defined(V8_TARGET_ARCH_X64) && !defined(V8_TARGET_ARCH_IA32) && \
    !defined(V8_TARGET_ARCH_ARM) && !defined(V8_TARGET_ARCH_MIPS)
#if defined(_M_X64) || defined(__x86_64__)
#define V8_TARGET_ARCH_X64 1
#elif defined(_M_IX86) || defined(__i386__)
#define V8_TARGET_ARCH_IA32 1
#elif defined(__ARMEL__)
#define V8_TARGET_ARCH_ARM 1
#elif defined(_MIPS_ARCH_MIPS32R2)
#define V8_TARGET_ARCH_MIPS 1
#else
#error Target architecture was not detected as supported by v8
#endif
#endif

// Check for supported combinations of host and target architectures.
#if defined(V8_TARGET_ARCH_IA32) && !defined(V8_HOST_ARCH_IA32)
#error Target architecture ia32 is only supported on ia32 host
#endif
#if defined(V8_TARGET_ARCH_X64) && !defined(V8_HOST_ARCH_X64)
#error Target architecture x64 is only supported on x64 host
#endif
#if (defined(V8_TARGET_ARCH_ARM) && \
    !(defined(V8_HOST_ARCH_IA32) || defined(V8_HOST_ARCH_ARM)))
#error Target architecture arm is only supported on arm and ia32 host
#endif
#if (defined(V8_TARGET_ARCH_MIPS) && \
    !(defined(V8_HOST_ARCH_IA32) || defined(V8_HOST_ARCH_MIPS)))
#error Target architecture mips is only supported on mips and ia32 host
#endif

// Define unaligned read for the target architectures supporting it.
#if defined(V8_TARGET_ARCH_X64) || defined(V8_TARGET_ARCH_IA32)
#define V8_TARGET_CAN_READ_UNALIGNED 1
#elif V8_TARGET_ARCH_ARM
// Some CPU-OS combinations allow unaligned access on ARM. We assume
// that unaligned accesses are not allowed unless the build system
// defines the CAN_USE_UNALIGNED_ACCESSES macro to be non-zero.
#if CAN_USE_UNALIGNED_ACCESSES
#define V8_TARGET_CAN_READ_UNALIGNED 1
#endif
#elif V8_TARGET_ARCH_MIPS
#else
#error Target architecture is not supported by v8
#endif

// The following macro works on both 32 and 64-bit platforms.
// Usage: instead of writing 0x1234567890123456
//      write V8_2PART_UINT64_C(0x12345678,90123456);
#define V8_2PART_UINT64_C(a, b) (((static_cast<uint64_t>(a) << 32) + 0x##b##u))

// -----------------------------------------------------------------------------
// Constants

const int kCharSize     = sizeof(char);      // NOLINT


// -----------------------------------------------------------------------------
// Macros


// The USE(x) template is used to silence C++ compiler warnings
// issued for (yet) unused variables (typically parameters).
template <typename T>
static inline void USE(T) { }


// A macro to disallow the evil copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define DISALLOW_COPY_AND_ASSIGN(TypeName)      \
  TypeName(const TypeName&);                    \
  void operator=(const TypeName&)


// A macro to disallow all the implicit constructors, namely the
// default constructor, copy constructor and operator= functions.
//
// This should be used in the private: declarations for a class
// that wants to prevent anyone from instantiating it. This is
// especially useful for classes containing only static methods.
#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName) \
  TypeName();                                    \
  DISALLOW_COPY_AND_ASSIGN(TypeName)

} }  // namespace v8::internal

#endif  // V8_GLOBALS_H_
