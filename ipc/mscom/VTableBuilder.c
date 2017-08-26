/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VTableBuilder.h"

#include <stdlib.h>

static HRESULT STDMETHODCALLTYPE
QueryInterfaceThunk(IUnknown* aThis, REFIID aIid, void** aOutInterface)
{
  void** table = (void**) aThis;
  IUnknown* real = (IUnknown*) table[1];
  return real->lpVtbl->QueryInterface(real, aIid, aOutInterface);
}

static ULONG STDMETHODCALLTYPE
AddRefThunk(IUnknown* aThis)
{
  void** table = (void**) aThis;
  IUnknown* real = (IUnknown*) table[1];
  return real->lpVtbl->AddRef(real);
}

static ULONG STDMETHODCALLTYPE
ReleaseThunk(IUnknown* aThis)
{
  void** table = (void**) aThis;
  IUnknown* real = (IUnknown*) table[1];
  return real->lpVtbl->Release(real);
}

IUnknown*
BuildNullVTable(IUnknown* aUnk, uint32_t aVtblSize)
{
  void** table;

  if (aVtblSize < 3) {
    return NULL;
  }

  // We need to allocate slots for two additional pointers: The |lpVtbl| pointer,
  // as well as |aUnk| which is needed to get |this| right when something calls
  // through one of the IUnknown thunks.
  table = calloc(aVtblSize + 2, sizeof(void*));

  table[0] = &table[2]; // |lpVtbl|, points to the first entry of the vtable
  table[1] = aUnk;      // |this|
  // Now the actual vtable entries for IUnknown
  table[2] = &QueryInterfaceThunk;
  table[3] = &AddRefThunk;
  table[4] = &ReleaseThunk;
  // Remaining entries are NULL thanks to calloc zero-initializing everything.

  return (IUnknown*) table;
}

void
DeleteNullVTable(IUnknown* aUnk)
{
  free(aUnk);
}

