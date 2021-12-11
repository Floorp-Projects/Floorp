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

#include <nr_api.h>
#include "nr_resolver.h"

int nr_resolver_create_int(void *obj, nr_resolver_vtbl *vtbl, nr_resolver **resolverp)
{
  int _status;
  nr_resolver *resolver=0;

  if (!(resolver=RCALLOC(sizeof(nr_resolver))))
    ABORT(R_NO_MEMORY);

  resolver->obj=obj;
  resolver->vtbl=vtbl;

  *resolverp=resolver;
  _status=0;
abort:
  return(_status);
}

int nr_resolver_destroy(nr_resolver **resolverp)
{
  nr_resolver *resolver;

  if(!resolverp || !*resolverp)
    return(0);

  resolver=*resolverp;
  *resolverp=0;

  resolver->vtbl->destroy(&resolver->obj);

  RFREE(resolver);

  return(0);
}

int nr_resolver_resolve(nr_resolver *resolver,
                        nr_resolver_resource *resource,
                        int (*cb)(void *cb_arg, nr_transport_addr *addr),
                        void *cb_arg,
                        void **handle)
{
  return resolver->vtbl->resolve(resolver->obj, resource, cb, cb_arg, handle);
}

int nr_resolver_cancel(nr_resolver *resolver, void *handle)
{
  return resolver->vtbl->cancel(resolver->obj, handle);
}
