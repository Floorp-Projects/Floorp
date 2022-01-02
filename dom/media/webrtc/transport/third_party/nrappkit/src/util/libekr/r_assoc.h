/**
   r_assoc.h


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
   r_assoc.h

   Associative array code. This code has the advantage that different
   elements can have different create and destroy operators. Unfortunately,
   this can waste space.


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

   $Id: r_assoc.h,v 1.3 2007/06/08 22:16:08 adamcain Exp $


   ekr@rtfm.com  Sun Jan 17 17:57:18 1999
 */


#ifndef _r_assoc_h
#define _r_assoc_h

typedef struct r_assoc_ r_assoc;

int r_assoc_create(r_assoc **assocp,
                             int (*hash_func)(char *,int,int),
                             int bits);
int r_assoc_insert(r_assoc *assoc,char *key,int len,
  void *value,int (*copy)(void **knew,void *old),
  int (*destroy)(void *ptr),int how);
#define R_ASSOC_REPLACE		  0x1
#define R_ASSOC_NEW		  0x2

int r_assoc_fetch(r_assoc *assoc,char *key, int len, void **value);
int r_assoc_delete(r_assoc *assoc,char *key, int len);

int r_assoc_copy(r_assoc **knew,r_assoc *old);
int r_assoc_destroy(r_assoc **assocp);
int r_assoc_simple_hash_compute(char *key, int len,int bits);
int r_assoc_crc32_hash_compute(char *key, int len,int bits);

/*We need iterators, but I haven't written them yet*/
typedef struct r_assoc_iterator_ {
     r_assoc *assoc;
     int prev_chain;
     struct r_assoc_el_ *prev;
     int next_chain;
     struct r_assoc_el_ *next;
} r_assoc_iterator;

int r_assoc_init_iter(r_assoc *assoc,r_assoc_iterator *);
int r_assoc_iter(r_assoc_iterator *iter,void **key,int *keyl, void **val);
int r_assoc_iter_delete(r_assoc_iterator *);

int r_assoc_num_elements(r_assoc *assoc,int *sizep);

#endif

