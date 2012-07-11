/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsMediaError.h"
#include "nsDOMClassInfoID.h"

NS_IMPL_ADDREF(nsMediaError)
NS_IMPL_RELEASE(nsMediaError)

DOMCI_DATA(MediaError, nsMediaError)

NS_INTERFACE_MAP_BEGIN(nsMediaError)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMediaError)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MediaError)
NS_INTERFACE_MAP_END

nsMediaError::nsMediaError(PRUint16 aCode) :
  mCode(aCode)
{
}

NS_IMETHODIMP nsMediaError::GetCode(PRUint16* aCode)
{
  if (aCode)
    *aCode = mCode;

  return NS_OK;
}
