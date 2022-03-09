/*
Copyright (c) 2007, Adobe Systems, Incorporated
Copyright (c) 2014, Mozilla
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

#ifndef _nr_socket_multi_tcp_h
#define _nr_socket_multi_tcp_h

#include "nr_socket.h"

/* Argument use_framing is 0 only in call from test code (STUN TCP server
   listening socket). For other purposes it should be always set to true */

int nr_socket_multi_tcp_create(struct nr_ice_ctx_ *ctx,
  struct nr_ice_component_ *component,
  nr_transport_addr *addr,  nr_socket_tcp_type tcp_type,
  int precreated_so_count, int max_pending, nr_socket **sockp);

int nr_socket_multi_tcp_set_readable_cb(nr_socket *sock,
  NR_async_cb readable_cb,void *readable_cb_arg);

int nr_socket_multi_tcp_stun_server_connect(nr_socket *sock,
  const nr_transport_addr *addr);

#endif
