/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "sdnDocAccessible.h"

#include "ISimpleDOMDocument_i.c"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// sdnDocAccessible
////////////////////////////////////////////////////////////////////////////////

IMPL_IUNKNOWN_QUERY_HEAD(sdnDocAccessible)
  IMPL_IUNKNOWN_QUERY_IFACE(ISimpleDOMDocument)
IMPL_IUNKNOWN_QUERY_TAIL_AGGREGATED(mAccessible)

STDMETHODIMP
sdnDocAccessible::get_URL(BSTR __RPC_FAR* aURL)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aURL)
    return E_INVALIDARG;
  *aURL = nullptr;

  if (mAccessible->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsAutoString URL;
  nsresult rv = mAccessible->GetURL(URL);
  if (NS_FAILED(rv))
    return E_FAIL;

  if (URL.IsEmpty())
    return S_FALSE;

  *aURL = ::SysAllocStringLen(URL.get(), URL.Length());
  return *aURL ? S_OK : E_OUTOFMEMORY;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
sdnDocAccessible::get_title(BSTR __RPC_FAR* aTitle)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aTitle)
    return E_INVALIDARG;
  *aTitle = nullptr;

  if (mAccessible->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsAutoString title;
  nsresult rv = mAccessible->GetTitle(title);
  if (NS_FAILED(rv))
    return E_FAIL;

  *aTitle = ::SysAllocStringLen(title.get(), title.Length());
  return *aTitle ? S_OK : E_OUTOFMEMORY;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
sdnDocAccessible::get_mimeType(BSTR __RPC_FAR* aMimeType)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aMimeType)
    return E_INVALIDARG;
  *aMimeType = nullptr;

  if (mAccessible->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsAutoString mimeType;
  nsresult rv = mAccessible->GetMimeType(mimeType);
  if (NS_FAILED(rv))
    return E_FAIL;

  if (mimeType.IsEmpty())
    return S_FALSE;

  *aMimeType = ::SysAllocStringLen(mimeType.get(), mimeType.Length());
  return *aMimeType ? S_OK : E_OUTOFMEMORY;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
sdnDocAccessible::get_docType(BSTR __RPC_FAR* aDocType)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aDocType)
    return E_INVALIDARG;
  *aDocType = nullptr;

  if (mAccessible->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsAutoString docType;
  nsresult rv = mAccessible->GetDocType(docType);
  if (NS_FAILED(rv))
    return E_FAIL;

  if (docType.IsEmpty())
    return S_FALSE;

  *aDocType = ::SysAllocStringLen(docType.get(), docType.Length());
  return *aDocType ? S_OK : E_OUTOFMEMORY;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
sdnDocAccessible::get_nameSpaceURIForID(short aNameSpaceID,
                                        BSTR __RPC_FAR* aNameSpaceURI)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aNameSpaceURI)
    return E_INVALIDARG;
  *aNameSpaceURI = nullptr;

  if (mAccessible->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  if (aNameSpaceID < 0)
    return E_INVALIDARG;  // -1 is kNameSpaceID_Unknown

  nsAutoString nameSpaceURI;
  nsresult rv = mAccessible->GetNameSpaceURIForID(aNameSpaceID, nameSpaceURI);
  if (NS_FAILED(rv))
    return E_FAIL;

  if (nameSpaceURI.IsEmpty())
    return S_FALSE;

  *aNameSpaceURI = ::SysAllocStringLen(nameSpaceURI.get(),
                                       nameSpaceURI.Length());

  return *aNameSpaceURI ? S_OK : E_OUTOFMEMORY;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
sdnDocAccessible::put_alternateViewMediaTypes(BSTR __RPC_FAR* aCommaSeparatedMediaTypes)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aCommaSeparatedMediaTypes)
    return E_INVALIDARG;
  *aCommaSeparatedMediaTypes = nullptr;

  return mAccessible->IsDefunct() ? CO_E_OBJNOTCONNECTED : E_NOTIMPL;

  A11Y_TRYBLOCK_END
}
