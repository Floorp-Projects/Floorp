/*
Copyright (c) 2007, Adobe Systems, Incorporated
Copyright (c) 2013, Mozilla
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

* Neither the name of Adobe Systems, Network Resonance nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/



#ifndef _nr_socket_buffered_stun_h
#define _nr_socket_buffered_stun_h

#include "nr_socket.h"

/* Wrapper socket which provides buffered STUN-oriented I/O

   1. Writes don't block and are automatically flushed when needed.
   2. All reads are in units of STUN messages

   This socket takes ownership of the inner socket |sock|.
 */

typedef enum {
  TURN_TCP_FRAMING=0,
  ICE_TCP_FRAMING
} nr_framing_type;

void nr_socket_buffered_stun_set_readable_cb(nr_socket *sock,
  NR_async_cb readable_cb, void *readable_cb_arg);

int nr_socket_buffered_stun_create(nr_socket *inner, int max_pending,
  nr_framing_type framing_type, nr_socket **sockp);

int nr_socket_buffered_set_connected_to(nr_socket *sock,
    nr_transport_addr *remote_addr);

#endif

