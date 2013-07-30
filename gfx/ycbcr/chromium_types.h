/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GFX_CHROMIUMTYPES_H
#define GFX_CHROMIUMTYPES_H

#include <stdint.h>

// On Windows, protypes.h is #included, which defines these types.  This sucks!
#ifndef PROTYPES_H
typedef uint8_t uint8;
typedef int8_t int8;
typedef int16_t int16;
typedef uint16_t uint16;
typedef uint32_t uint32;
#endif

// From Chromium build_config.h:
// Processor architecture detection.  For more info on what's defined, see:
//   http://msdn.microsoft.com/en-us/library/b0084kay.aspx
//   http://www.agner.org/optimize/calling_conventions.pdf
//   or with gcc, run: "echo | gcc -E -dM -"
#if defined(_M_X64) || defined(__x86_64__)
#define ARCH_CPU_X86_FAMILY 1
#define ARCH_CPU_X86_64 1
#define ARCH_CPU_64_BITS 1
#elif defined(_M_IX86) || defined(__i386__) || defined(__i386)
#define ARCH_CPU_X86_FAMILY 1
#define ARCH_CPU_X86_32 1
#define ARCH_CPU_X86 1
#define ARCH_CPU_32_BITS 1
#elif defined(__ARMEL__)
#define ARCH_CPU_ARM_FAMILY 1
#define ARCH_CPU_ARMEL 1
#define ARCH_CPU_32_BITS 1
#elif defined(__ppc__) || defined(__powerpc) || defined(__PPC__)
#define ARCH_CPU_PPC_FAMILY 1
#define ARCH_CPU_PPC 1
#define ARCH_CPU_32_BITS 1
#elif defined(__sparc)
#define ARCH_CPU_SPARC_FAMILY 1
#define ARCH_CPU_SPARC 1
#define ARCH_CPU_32_BITS 1
#elif defined(__sparcv9)
#define ARCH_CPU_SPARC_FAMILY 1
#define ARCH_CPU_SPARC 1
#define ARCH_CPU_64_BITS 1
#else
#warning Please add support for your architecture in chromium_types.h
#endif

#endif // GFX_CHROMIUMTYPES_H
