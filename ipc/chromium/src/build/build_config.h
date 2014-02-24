// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file adds defines about the platform we're currently building on.
//  Operating System:
//    OS_WIN / OS_MACOSX / OS_LINUX / OS_POSIX (MACOSX or LINUX)
//  Compiler:
//    COMPILER_MSVC / COMPILER_GCC
//  Processor:
//    ARCH_CPU_X86 / ARCH_CPU_X86_64 / ARCH_CPU_X86_FAMILY (X86 or X86_64)
//    ARCH_CPU_32_BITS / ARCH_CPU_64_BITS

#ifndef BUILD_BUILD_CONFIG_H_
#define BUILD_BUILD_CONFIG_H_

// A set of macros to use for platform detection.
#if defined(ANDROID)
#define OS_ANDROID 1
#define OS_LINUX 1
#elif defined(__APPLE__)
#define OS_MACOSX 1
#elif defined(__linux__)
#define OS_LINUX 1
#elif defined(__DragonFly__)
#define OS_DRAGONFLY 1
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#define OS_FREEBSD 1
#elif defined(__NetBSD__)
#define OS_NETBSD 1
#elif defined(__OpenBSD__)
#define OS_OPENBSD 1
#elif defined(_WIN32)
#define OS_WIN 1
#else
#error Please add support for your platform in build/build_config.h
#endif

// For access to standard BSD features, use OS_BSD instead of a
// more specific macro.
#if defined(OS_DRAGONFLY) || defined(OS_FREEBSD)	\
  || defined(OS_NETBSD) || defined(OS_OPENBSD)
#define OS_BSD 1
#endif

// For access to standard POSIX features, use OS_POSIX instead of a more
// specific macro.
#if defined(OS_MACOSX) || defined(OS_LINUX) || defined(OS_BSD)
#define OS_POSIX 1
#endif

// Compiler detection.
#if defined(__GNUC__)
#define COMPILER_GCC 1
#elif defined(_MSC_VER)
#define COMPILER_MSVC 1
#else
#error Please add support for your compiler in build/build_config.h
#endif

// Processor architecture detection.  For more info on what's defined, see:
//   http://msdn.microsoft.com/en-us/library/b0084kay.aspx
//   http://www.agner.org/optimize/calling_conventions.pdf
//   or with gcc, run: "echo | gcc -E -dM -"
#if defined(_M_X64) || defined(__x86_64__)
#define ARCH_CPU_X86_FAMILY 1
#define ARCH_CPU_X86_64 1
#define ARCH_CPU_64_BITS 1
#elif defined(_M_IX86) || defined(__i386__)
#define ARCH_CPU_X86_FAMILY 1
#define ARCH_CPU_X86 1
#define ARCH_CPU_32_BITS 1
#elif defined(__ARMEL__)
#define ARCH_CPU_ARM_FAMILY 1
#define ARCH_CPU_ARMEL 1
#define ARCH_CPU_32_BITS 1
#define WCHAR_T_IS_UNSIGNED 1
#elif defined(__powerpc64__)
#define ARCH_CPU_PPC64 1
#define ARCH_CPU_64_BITS 1
#elif defined(__ppc__) || defined(__powerpc__)
#define ARCH_CPU_PPC 1
#define ARCH_CPU_32_BITS 1
#elif defined(__sparc64__)
#define ARCH_CPU_SPARC 1
#define ARCH_CPU_64_BITS 1
#elif defined(__sparc__)
#define ARCH_CPU_SPARC 1
#define ARCH_CPU_32_BITS 1
#elif defined(__mips__)
#define ARCH_CPU_MIPS 1
#define ARCH_CPU_32_BITS 1
#elif defined(__hppa__)
#define ARCH_CPU_HPPA 1
#define ARCH_CPU_32_BITS 1
#elif defined(__ia64__)
#define ARCH_CPU_IA64 1
#define ARCH_CPU_64_BITS 1
#elif defined(__s390x__)
#define ARCH_CPU_S390X 1
#define ARCH_CPU_64_BITS 1
#elif defined(__s390__)
#define ARCH_CPU_S390 1
#define ARCH_CPU_32_BITS 1
#elif defined(__alpha__)
#define ARCH_CPU_ALPHA 1
#define ARCH_CPU_64_BITS 1
#elif defined(__aarch64__)
#define ARCH_CPU_AARCH64 1
#define ARCH_CPU_64_BITS 1
#else
#error Please add support for your architecture in build/build_config.h
#endif

// Type detection for wchar_t.
#if defined(OS_WIN)
#define WCHAR_T_IS_UTF16
#else
#define WCHAR_T_IS_UTF32
#endif

#endif  // BUILD_BUILD_CONFIG_H_
