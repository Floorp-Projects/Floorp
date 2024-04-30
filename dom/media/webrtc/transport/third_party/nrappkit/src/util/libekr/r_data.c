/**
   r_data.c


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
   r_data.c


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

   $Id: r_data.c,v 1.2 2006/08/16 19:39:17 adamcain Exp $

   ekr@rtfm.com  Tue Aug 17 15:39:50 1999
 */

#include <string.h>
#include <r_common.h>
#include <r_data.h>
#include <string.h>

int r_data_create(dp,d,l)
  Data **dp;
  const UCHAR *d;
  size_t l;
  {
    Data *d_=0;
    int _status;

    if(!(d_=(Data *)RCALLOC(sizeof(Data))))
      ABORT(R_NO_MEMORY);
    if(!(d_->data=(UCHAR *)RMALLOC(l)))
      ABORT(R_NO_MEMORY);

    if (d) {
      memcpy(d_->data,d,l);
    }
    d_->len=l;

    *dp=d_;

    _status=0;
  abort:
    if(_status)
      r_data_destroy(&d_);

    return(_status);
  }


int r_data_alloc_mem(d,l)
  Data *d;
  size_t l;
  {
    int _status;

    if(!(d->data=(UCHAR *)RMALLOC(l)))
      ABORT(R_NO_MEMORY);
    d->len=l;

    _status=0;
  abort:
    return(_status);
  }

int r_data_alloc(dp,l)
  Data **dp;
  size_t l;
  {
    Data *d_=0;
    int _status;

    if(!(d_=(Data *)RCALLOC(sizeof(Data))))
      ABORT(R_NO_MEMORY);
    if(!(d_->data=(UCHAR *)RCALLOC(l)))
      ABORT(R_NO_MEMORY);

    d_->len=l;

    *dp=d_;
    _status=0;
  abort:
    if(_status)
      r_data_destroy(&d_);

    return(_status);
  }

int r_data_make(dp,d,l)
  Data *dp;
  const UCHAR *d;
  size_t l;
  {
    if(!(dp->data=(UCHAR *)RMALLOC(l)))
      ERETURN(R_NO_MEMORY);

    memcpy(dp->data,d,l);
    dp->len=l;

    return(0);
  }

int r_data_destroy(dp)
  Data **dp;
  {
    if(!dp || !*dp)
      return(0);

    if((*dp)->data)
      RFREE((*dp)->data);

    RFREE(*dp);
    *dp=0;

    return(0);
  }

int r_data_destroy_v(v)
  void *v;
  {
    Data *d = 0;

    if(!v)
      return(0);

    d=(Data *)v;
    r_data_zfree(d);

    RFREE(d);

    return(0);
  }

int r_data_destroy_vp(v)
  void **v;
  {
    Data *d = 0;

    if(!v || !*v)
      return(0);

    d=(Data *)*v;
    r_data_zfree(d);

    *v=0;
    RFREE(d);

    return(0);
  }

int r_data_copy(dst,src)
  Data *dst;
  Data *src;
  {
    if(!(dst->data=(UCHAR *)RMALLOC(src->len)))
      ERETURN(R_NO_MEMORY);
    memcpy(dst->data,src->data,dst->len=src->len);
    return(0);
  }

int r_data_zfree(d)
  Data *d;
  {
    if(!d)
      return(0);
    if(!d->data)
      return(0);
    memset(d->data,0,d->len);
    RFREE(d->data);
    return(0);
  }

int r_data_compare(d1,d2)
  Data *d1;
  Data *d2;
  {
    if(d1->len<d2->len)
      return(-1);
    if(d2->len<d1->len)
      return(-1);
    return(memcmp(d1->data,d2->data,d1->len));
  }

