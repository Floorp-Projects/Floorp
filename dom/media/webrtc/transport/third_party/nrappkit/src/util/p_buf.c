/**
   p_buf.c


   Copyright (C) 2003, Network Resonance, Inc.
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


   All Rights Reserved.

   ekr@rtfm.com  Tue Nov 25 16:33:08 2003
 */

#include <string.h>
#include <stddef.h>
#include "nr_common.h"
#include "p_buf.h"


static int nr_p_buf_destroy_chain(nr_p_buf_head *head);
static int nr_p_buf_destroy(nr_p_buf *buf);

int nr_p_buf_ctx_create(size,ctxp)
  int size;
  nr_p_buf_ctx **ctxp;
  {
    int _status;
    nr_p_buf_ctx *ctx=0;

    if(!(ctx=(nr_p_buf_ctx *)RCALLOC(sizeof(nr_p_buf_ctx))))
      ABORT(R_NO_MEMORY);

    ctx->buf_size=size;
    STAILQ_INIT(&ctx->free_list);

    *ctxp=ctx;
    _status=0;
  abort:
    if(_status){
      nr_p_buf_ctx_destroy(&ctx);
    }
    return(_status);
  }

int nr_p_buf_ctx_destroy(ctxp)
  nr_p_buf_ctx **ctxp;
  {
    nr_p_buf_ctx *ctx;

    if(!ctxp || !*ctxp)
      return(0);

    ctx=*ctxp;

    nr_p_buf_destroy_chain(&ctx->free_list);

    RFREE(ctx);
    *ctxp=0;

    return(0);
  }

int nr_p_buf_alloc(ctx,bufp)
  nr_p_buf_ctx *ctx;
  nr_p_buf **bufp;
  {
    int _status;
    nr_p_buf *buf=0;

    if(!STAILQ_EMPTY(&ctx->free_list)){
      buf=STAILQ_FIRST(&ctx->free_list);
      STAILQ_REMOVE_HEAD(&ctx->free_list,entry);
      goto ok;
    }
    else {
      if(!(buf=(nr_p_buf *)RCALLOC(sizeof(nr_p_buf))))
        ABORT(R_NO_MEMORY);
      if(!(buf->data=(UCHAR *)RMALLOC(ctx->buf_size)))
        ABORT(R_NO_MEMORY);
      buf->size=ctx->buf_size;
    }

  ok:
     buf->r_offset=0;
     buf->length=0;

     *bufp=buf;
    _status=0;
  abort:
     if(_status){
       nr_p_buf_destroy(buf);
     }
    return(_status);
  }

int nr_p_buf_free(ctx,buf)
  nr_p_buf_ctx *ctx;
  nr_p_buf *buf;
  {
    STAILQ_INSERT_TAIL(&ctx->free_list,buf,entry);

    return(0);
  }

int nr_p_buf_free_chain(ctx,head)
  nr_p_buf_ctx *ctx;
  nr_p_buf_head *head;
  {
    nr_p_buf *n1,*n2;

    n1=STAILQ_FIRST(head);
    while(n1){
      n2=STAILQ_NEXT(n1,entry);

      nr_p_buf_free(ctx,n1);

      n1=n2;
    }

    return(0);
  }


int nr_p_buf_write_to_chain(ctx,chain,data,len)
  nr_p_buf_ctx *ctx;
  nr_p_buf_head *chain;
  UCHAR *data;
  UINT4 len;
  {
    int r,_status;
    nr_p_buf *buf;

    buf=STAILQ_LAST(chain,nr_p_buf_,entry);
    while(len){
      int towrite;

      if(!buf){
        if(r=nr_p_buf_alloc(ctx,&buf))
          ABORT(r);
        STAILQ_INSERT_TAIL(chain,buf,entry);
      }

      towrite=MIN(len,(buf->size-(buf->length+buf->r_offset)));

      memcpy(buf->data+buf->length+buf->r_offset,data,towrite);
      len-=towrite;
      data+=towrite;
      buf->length+=towrite;

      r_log(LOG_COMMON,LOG_DEBUG,"Wrote %d bytes to buffer %p",towrite,buf);
      buf=0;
    }

    _status=0;
  abort:
    return(_status);
  }

static int nr_p_buf_destroy_chain(head)
  nr_p_buf_head *head;
  {
    nr_p_buf *n1,*n2;

    n1=STAILQ_FIRST(head);
    while(n1){
      n2=STAILQ_NEXT(n1,entry);

      nr_p_buf_destroy(n1);

      n1=n2;
    }

    return(0);
  }

static int nr_p_buf_destroy(buf)
  nr_p_buf *buf;
  {
    if(!buf)
      return(0);

    RFREE(buf->data);
    RFREE(buf);

    return(0);
  }


