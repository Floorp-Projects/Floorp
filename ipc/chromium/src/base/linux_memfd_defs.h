/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BASE_LINUX_MEMFD_DEFS_H
#define BASE_LINUX_MEMFD_DEFS_H

#include <sys/syscall.h>

// glibc before 2.27 didn't have a memfd_create wrapper, and if the
// build system is old enough then it won't have the syscall number
// and various related constants either.

#if defined(__x86_64__)
#  define MEMFD_CREATE_NR 319
#elif defined(__i386__)
#  define MEMFD_CREATE_NR 356
#elif defined(__aarch64__)
#  define MEMFD_CREATE_NR 279
#elif defined(__arm__)
#  define MEMFD_CREATE_NR 385
#elif defined(__powerpc__)
#  define MEMFD_CREATE_NR 360
#elif defined(__s390__)
#  define MEMFD_CREATE_NR 350
#elif defined(__mips__)
#  include <sgidefs.h>
#  if _MIPS_SIM == _MIPS_SIM_ABI32
#    define MEMFD_CREATE_NR 4354
#  elif _MIPS_SIM == _MIPS_SIM_ABI64
#    define MEMFD_CREATE_NR 5314
#  elif _MIPS_SIM == _MIPS_SIM_NABI32
#    define MEMFD_CREATE_NR 6318
#  endif  // mips subarch
#endif    // arch

#ifdef MEMFD_CREATE_NR
#  ifdef SYS_memfd_create
static_assert(MEMFD_CREATE_NR == SYS_memfd_create,
              "MEMFD_CREATE_NR should match the actual SYS_memfd_create value");
#  else  // defined here but not in system headers
#    define SYS_memfd_create MEMFD_CREATE_NR
#  endif
#endif

#ifndef MFD_CLOEXEC
#  define MFD_CLOEXEC 0x0001U
#  define MFD_ALLOW_SEALING 0x0002U
#endif

#ifndef F_ADD_SEALS
#  ifndef F_LINUX_SPECIFIC_BASE
#    define F_LINUX_SPECIFIC_BASE 1024
#  endif
#  define F_ADD_SEALS (F_LINUX_SPECIFIC_BASE + 9)
#  define F_GET_SEALS (F_LINUX_SPECIFIC_BASE + 10)
#  define F_SEAL_SEAL 0x0001   /* prevent further seals from being set */
#  define F_SEAL_SHRINK 0x0002 /* prevent file from shrinking */
#  define F_SEAL_GROW 0x0004   /* prevent file from growing */
#  define F_SEAL_WRITE 0x0008  /* prevent writes */
#endif

#ifndef F_SEAL_FUTURE_WRITE
#  define F_SEAL_FUTURE_WRITE 0x0010
#endif

#endif  // BASE_LINUX_MEMFD_DEFS_H
