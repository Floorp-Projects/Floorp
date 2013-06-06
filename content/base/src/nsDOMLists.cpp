/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of nsIDOMDOMStringList, used by various DOM stuff.
 */

#include "nsDOMLists.h"
#include "nsError.h"
#include "nsDOMClassInfoID.h"
#include "nsINode.h"

nsDOMStringList::nsDOMStringList()
{
}

nsDOMStringList::~nsDOMStringList()
{
}

DOMCI_DATA(DOMStringList, nsDOMStringList)

NS_IMPL_ADDREF(nsDOMStringList)
NS_IMPL_RELEASE(nsDOMStringList)
NS_INTERFACE_TABLE_HEAD(nsDOMStringList)
  NS_INTERFACE_TABLE1(nsDOMStringList, nsIDOMDOMStringList)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(DOMStringList)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
nsDOMStringList::Item(uint32_t aIndex, nsAString& aResult)
{
  if (aIndex >= mNames.Length()) {
    SetDOMStringToNull(aResult);
  } else {
    aResult = mNames[aIndex];
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMStringList::GetLength(uint32_t *aLength)
{
  *aLength = mNames.Length();

  return NS_OK;
}

NS_IMETHODIMP
nsDOMStringList::Contains(const nsAString& aString, bool *aResult)
{
  *aResult = mNames.Contains(aString);

  return NS_OK;
}
