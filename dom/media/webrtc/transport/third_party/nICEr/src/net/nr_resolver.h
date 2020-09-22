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

* Neither the name of Adobe Systems, Network Resonance, Mozilla nor
  the names of its contributors may be used to endorse or promote
  products derived from this software without specific prior written
  permission.

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

#ifndef _nr_resolver_h
#define _nr_resolver_h

#include "transport_addr.h"

#define NR_RESOLVE_PROTOCOL_STUN 1
#define NR_RESOLVE_PROTOCOL_TURN 2

typedef struct nr_resolver_resource_ {
  const char *domain_name;
  UINT2 port;
  int stun_turn;
  UCHAR transport_protocol;
  UCHAR address_family;
} nr_resolver_resource;

typedef struct nr_resolver_vtbl_ {
  int (*destroy)(void **obj);
  int (*resolve)(void *obj,
                 nr_resolver_resource *resource,
                 int (*cb)(void *cb_arg, nr_transport_addr *addr),
                 void *cb_arg,
                 void **handle);
  int (*cancel)(void *obj, void *handle);
} nr_resolver_vtbl;

typedef struct nr_resolver_ {
  void *obj;
  nr_resolver_vtbl *vtbl;
} nr_resolver;


/*
  The convention here is that the provider of this interface
  must generate a void *obj, and a vtbl and then call
  nr_resolver_create_int() to allocate the generic wrapper
  object.

  The vtbl must contain implementations for all the functions
  listed.

  The nr_resolver_destroy() function (and hence vtbl->destroy)
  will be called when the consumer of the resolver is done
  with it. That is the signal that it is safe to clean up
  the resources associated with obj. No other function will
  be called afterwards.
*/
int nr_resolver_create_int(void *obj, nr_resolver_vtbl *vtbl,
                           nr_resolver **resolverp);
int nr_resolver_destroy(nr_resolver **resolverp);

/* Request resolution of a domain */
int nr_resolver_resolve(nr_resolver *resolver,
                        nr_resolver_resource *resource,
                        int (*cb)(void *cb_arg, nr_transport_addr *addr),
                        void *cb_arg,
                        void **handle);

/* Cancel a requested resolution. No callback will fire. */
int nr_resolver_cancel(nr_resolver *resolver, void *handle);
#endif
