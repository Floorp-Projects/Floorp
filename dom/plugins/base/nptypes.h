/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nptypes_h_
#define nptypes_h_

/*
 * Header file for ensuring that C99 types ([u]int32_t, [u]int64_t and bool) and
 * true/false macros are available.
 */

#if defined(WIN32) || defined(OS2)
  /*
   * Win32 and OS/2 don't know C99, so define [u]int_16/32/64 here. The bool
   * is predefined tho, both in C and C++.
   */
  typedef short int16_t;
  typedef unsigned short uint16_t;
  typedef int int32_t;
  typedef unsigned int uint32_t;
  typedef long long int64_t;
  typedef unsigned long long uint64_t;
#elif defined(_AIX) || defined(__sun) || defined(__osf__) || defined(IRIX) || defined(HPUX)
  /*
   * AIX and SunOS ship a inttypes.h header that defines [u]int32_t,
   * but not bool for C.
   */
  #include <inttypes.h>

  #ifndef __cplusplus
    typedef int bool;
    #define true   1
    #define false  0
  #endif
#elif defined(bsdi) || defined(FREEBSD) || defined(OPENBSD)
  /*
   * BSD/OS, FreeBSD, and OpenBSD ship sys/types.h that define int32_t and
   * u_int32_t.
   */
  #include <sys/types.h>

  /*
   * BSD/OS ships no header that defines uint32_t, nor bool (for C)
   */
  #if defined(bsdi)
  typedef u_int32_t uint32_t;
  typedef u_int64_t uint64_t;

  #if !defined(__cplusplus)
    typedef int bool;
    #define true   1
    #define false  0
  #endif
  #else
  /*
   * FreeBSD and OpenBSD define uint32_t and bool.
   */
    #include <inttypes.h>
    #include <stdbool.h>
  #endif
#elif defined(BEOS)
  #include <inttypes.h>
#else
  /*
   * For those that ship a standard C99 stdint.h header file, include
   * it. Can't do the same for stdbool.h tho, since some systems ship
   * with a stdbool.h file that doesn't compile!
   */
  #include <stdint.h>

  #ifndef __cplusplus
    #if !defined(__GNUC__) || (__GNUC__ > 2 || __GNUC_MINOR__ > 95)
      #include <stdbool.h>
    #else
      /*
       * GCC 2.91 can't deal with a typedef for bool, but a #define
       * works.
       */
      #define bool int
      #define true   1
      #define false  0
    #endif
  #endif
#endif

#endif /* nptypes_h_ */
