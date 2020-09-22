/**
   r_errors.h


   Copyright (C) 2002-2003, Network Resonance, Inc.
   Copyright (C) 2006, Network Resonance, Inc.
   All Rights Reserved

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. Neither the name of Network Resonance, Inc. nor the name of any
      contributors to this software may be used to endorse or promote
      products derived from this software without specific prior written
      permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.


 */

/**
   r_errors.h


   Copyright (C) 1999-2000 RTFM, Inc.
   All Rights Reserved

   This package is a SSLv3/TLS protocol analyzer written by Eric Rescorla
   <ekr@rtfm.com> and licensed by RTFM, Inc.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. All advertising materials mentioning features or use of this software
      must display the following acknowledgement:

      This product includes software developed by Eric Rescorla for
      RTFM, Inc.

   4. Neither the name of RTFM, Inc. nor the name of Eric Rescorla may be
      used to endorse or promote products derived from this
      software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY ERIC RESCORLA AND RTFM, INC. ``AS IS'' AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY SUCH DAMAGE.

   $Id: r_errors.h,v 1.4 2007/10/12 20:53:24 adamcain Exp $


   ekr@rtfm.com  Tue Dec 22 10:59:49 1998
 */


#ifndef _r_errors_h
#define _r_errors_h

#define R_NO_MEMORY      1 /*out of memory*/
#define R_NOT_FOUND	 2 /*Item not found*/
#define R_INTERNAL	 3 /*Unspecified internal error*/
#define R_ALREADY	 4 /*Action already done*/
#define R_EOD            5 /*end of data*/
#define R_BAD_ARGS       6 /*Bad arguments*/
#define R_BAD_DATA	 7 /*Bad data*/
#define R_WOULDBLOCK     8 /*Operation would block */
#define R_QUEUED         9 /*Operation was queued */
#define R_FAILED        10 /*Operation failed */
#define R_REJECTED      11 /* We don't care about this */
#define R_INTERRUPTED   12 /* Operation interrupted */
#define R_IO_ERROR      13 /* I/O Error */
#define R_NOT_PERMITTED 14 /* Permission denied */
#define R_RETRY         15 /* Retry possible */

#define NR_ERROR_MAPPING {\
    { R_NO_MEMORY,        "Cannot allocate memory" },\
    { R_NOT_FOUND,        "Item not found" },\
    { R_INTERNAL,         "Internal failure" },\
    { R_ALREADY,          "Action already performed" },\
    { R_EOD,              "End of data" },\
    { R_BAD_ARGS,         "Invalid argument" },\
    { R_BAD_DATA,         "Invalid data" },\
    { R_WOULDBLOCK,       "Operation would block" },\
    { R_QUEUED,           "Operation queued" },\
    { R_FAILED,           "Operation failed" },\
    { R_REJECTED,         "Operation rejected" },\
    { R_INTERRUPTED,      "Operation interrupted" },\
    { R_IO_ERROR,         "I/O error" },\
    { R_NOT_PERMITTED,    "Permission Denied" },\
    { R_RETRY,            "Retry may be possible" },\
    }

int nr_verr_exit(char *fmt,...);

char *nr_strerror(int errnum);
int   nr_strerror_r(int errnum, char *strerrbuf, size_t buflen);

#endif

