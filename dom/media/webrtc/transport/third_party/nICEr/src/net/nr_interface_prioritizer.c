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

#include "nr_api.h"
#include "nr_interface_prioritizer.h"
#include "transport_addr.h"

int nr_interface_prioritizer_create_int(void *obj,
  nr_interface_prioritizer_vtbl *vtbl,nr_interface_prioritizer **ifpp)
  {
    int _status;
    nr_interface_prioritizer *ifp=0;

    if(!(ifp=RCALLOC(sizeof(nr_interface_prioritizer))))
      ABORT(R_NO_MEMORY);

    ifp->obj = obj;
    ifp->vtbl = vtbl;

    *ifpp = ifp;

    _status=0;
  abort:
    return(_status);
  }

int nr_interface_prioritizer_destroy(nr_interface_prioritizer **ifpp)
  {
    nr_interface_prioritizer *ifp;

    if (!ifpp || !*ifpp)
      return(0);

    ifp = *ifpp;
    *ifpp = 0;
    ifp->vtbl->destroy(&ifp->obj);
    RFREE(ifp);
    return(0);
  }

int nr_interface_prioritizer_add_interface(nr_interface_prioritizer *ifp,
  nr_local_addr *addr)
  {
    return ifp->vtbl->add_interface(ifp->obj, addr);
  }

int nr_interface_prioritizer_get_priority(nr_interface_prioritizer *ifp,
  const char *key, UCHAR *interface_preference)
  {
    return ifp->vtbl->get_priority(ifp->obj,key,interface_preference);
  }

int nr_interface_prioritizer_sort_preference(nr_interface_prioritizer *ifp)
  {
    return ifp->vtbl->sort_preference(ifp->obj);
  }
