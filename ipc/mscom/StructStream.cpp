/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <malloc.h>
#include <rpc.h>

/**
 * These functions need to be defined in order for the types that use
 * mozilla::mscom::StructToStream and mozilla::mscom::StructFromStream to work.
 */
extern "C" {

void __RPC_FAR* __RPC_USER
midl_user_allocate(size_t aNumBytes)
{
  const unsigned long kRpcReqdBufAlignment = 8;
  return _aligned_malloc(aNumBytes, kRpcReqdBufAlignment);
}

void __RPC_USER
midl_user_free(void* aBuffer)
{
  _aligned_free(aBuffer);
}

} // extern "C"
