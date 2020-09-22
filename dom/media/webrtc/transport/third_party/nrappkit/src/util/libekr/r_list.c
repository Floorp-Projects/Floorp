/**
   r_list.c


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
   r_list.c


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

   $Id: r_list.c,v 1.2 2006/08/16 19:39:17 adamcain Exp $


   ekr@rtfm.com  Tue Jan 19 08:36:39 1999
 */

#include <r_common.h>
#include "r_list.h"

typedef struct r_list_el_ {
     void *data;
     struct r_list_el_ *next;
     struct r_list_el_ *prev;
     int (*copy)(void **new,void *old);
     int (*destroy)(void **ptr);
} r_list_el;

struct r_list_ {
     struct r_list_el_ *first;
     struct r_list_el_ *last;
};

int r_list_create(listp)
  r_list **listp;
  {
    r_list *list=0;
    int _status;

    if(!(list=(r_list *)RCALLOC(sizeof(r_list))))
      ABORT(R_NO_MEMORY);

    list->first=0;
    list->last=0;
    *listp=list;

    _status=0;
  abort:
    return(_status);
  }

int r_list_destroy(listp)
  r_list **listp;
  {
    r_list *list;
    r_list_el *el;

    if(!listp || !*listp)
      return(0);
    list=*listp;

    el=list->first;

    while(el){
      r_list_el *el_t;

      if(el->destroy && el->data)
        el->destroy(&el->data);
      el_t=el;
      el=el->next;
      RFREE(el_t);
    }

    RFREE(list);
    *listp=0;

    return(0);
  }

int r_list_copy(outp,in)
  r_list**outp;
  r_list *in;
  {
    r_list *out=0;
    r_list_el *el,*el2,*last=0;
    int r, _status;

    if(!in){
      *outp=0;
      return(0);
    }

    if(r=r_list_create(&out))
      ABORT(r);

    for(el=in->first;el;el=el->next){
      if(!(el2=(r_list_el *)RCALLOC(sizeof(r_list_el))))
        ABORT(R_NO_MEMORY);

      if(el->copy && el->data){
        if(r=el->copy(&el2->data,el->data))
          ABORT(r);
      }

      el2->copy=el->copy;
      el2->destroy=el->destroy;

      if(!(out->first))
        out->first=el2;

      el2->prev=last;
      if(last) last->next=el2;
      last=el2;
    }

    out->last=last;

    *outp=out;

    _status=0;
  abort:
    if(_status)
      r_list_destroy(&out);
    return(_status);
  }

int r_list_insert(list,value,copy,destroy)
  r_list *list;
  void *value;
  int (*copy)(void **out, void *in);
  int (*destroy)(void **val);
  {
    r_list_el *el=0;
    int _status;

    if(!(el=(r_list_el *)RCALLOC(sizeof(r_list_el))))
      ABORT(R_NO_MEMORY);
    el->data=value;
    el->copy=copy;
    el->destroy=destroy;

    el->prev=0;
    el->next=list->first;
    if(list->first){
      list->first->prev=el;
    }
    list->first=el;

    _status=0;
  abort:
    return(_status);
  }

int r_list_append(list,value,copy,destroy)
  r_list *list;
  void *value;
  int (*copy)(void **out, void *in);
  int (*destroy)(void **val);
  {
    r_list_el *el=0;
    int _status;

    if(!(el=(r_list_el *)RCALLOC(sizeof(r_list_el))))
      ABORT(R_NO_MEMORY);
    el->data=value;
    el->copy=copy;
    el->destroy=destroy;

    el->prev=list->last;
    el->next=0;

    if(list->last) list->last->next=el;
    else list->first=el;

    list->last=el;

    _status=0;
  abort:
    return(_status);
  }

int r_list_init_iter(list,iter)
  r_list *list;
  r_list_iterator *iter;
  {
    iter->list=list;
    iter->ptr=list->first;

    return(0);
  }

int r_list_iter(iter,val)
  r_list_iterator *iter;
  void **val;
  {
    if(!iter->ptr)
      return(R_EOD);

    *val=iter->ptr->data;
    iter->ptr=iter->ptr->next;

    return(0);
  }





