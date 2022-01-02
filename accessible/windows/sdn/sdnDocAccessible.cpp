/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "sdnDocAccessible.h"

#include "LocalAccessible-inl.h"
#include "ISimpleDOM.h"

#include "nsNameSpaceManager.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// sdnDocAccessible
////////////////////////////////////////////////////////////////////////////////

IMPL_IUNKNOWN_QUERY_HEAD(sdnDocAccessible)
IMPL_IUNKNOWN_QUERY_IFACE(ISimpleDOMDocument)
IMPL_IUNKNOWN_QUERY_TAIL_AGGREGATED(mMsaa)

STDMETHODIMP
sdnDocAccessible::get_URL(BSTR __RPC_FAR* aURL) {
  if (!aURL) return E_INVALIDARG;
  *aURL = nullptr;

  DocAccessible* acc = mMsaa->DocAcc();
  if (!acc) return CO_E_OBJNOTCONNECTED;

  nsAutoString URL;
  acc->URL(URL);
  if (URL.IsEmpty()) return S_FALSE;

  *aURL = ::SysAllocStringLen(URL.get(), URL.Length());
  return *aURL ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP
sdnDocAccessible::get_title(BSTR __RPC_FAR* aTitle) {
  if (!aTitle) return E_INVALIDARG;
  *aTitle = nullptr;

  DocAccessible* acc = mMsaa->DocAcc();
  if (!acc) return CO_E_OBJNOTCONNECTED;

  nsAutoString title;
  acc->Title(title);
  *aTitle = ::SysAllocStringLen(title.get(), title.Length());
  return *aTitle ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP
sdnDocAccessible::get_mimeType(BSTR __RPC_FAR* aMimeType) {
  if (!aMimeType) return E_INVALIDARG;
  *aMimeType = nullptr;

  DocAccessible* acc = mMsaa->DocAcc();
  if (!acc) return CO_E_OBJNOTCONNECTED;

  nsAutoString mimeType;
  acc->MimeType(mimeType);
  if (mimeType.IsEmpty()) return S_FALSE;

  *aMimeType = ::SysAllocStringLen(mimeType.get(), mimeType.Length());
  return *aMimeType ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP
sdnDocAccessible::get_docType(BSTR __RPC_FAR* aDocType) {
  if (!aDocType) return E_INVALIDARG;
  *aDocType = nullptr;

  DocAccessible* acc = mMsaa->DocAcc();
  if (!acc) return CO_E_OBJNOTCONNECTED;

  nsAutoString docType;
  acc->DocType(docType);
  if (docType.IsEmpty()) return S_FALSE;

  *aDocType = ::SysAllocStringLen(docType.get(), docType.Length());
  return *aDocType ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP
sdnDocAccessible::get_nameSpaceURIForID(short aNameSpaceID,
                                        BSTR __RPC_FAR* aNameSpaceURI) {
  if (!aNameSpaceURI) return E_INVALIDARG;
  *aNameSpaceURI = nullptr;

  if (!mMsaa->DocAcc()) return CO_E_OBJNOTCONNECTED;

  if (aNameSpaceID < 0) return E_INVALIDARG;  // -1 is kNameSpaceID_Unknown

  nsAutoString nameSpaceURI;
  nsNameSpaceManager* nameSpaceManager = nsNameSpaceManager::GetInstance();
  if (nameSpaceManager)
    nameSpaceManager->GetNameSpaceURI(aNameSpaceID, nameSpaceURI);

  if (nameSpaceURI.IsEmpty()) return S_FALSE;

  *aNameSpaceURI =
      ::SysAllocStringLen(nameSpaceURI.get(), nameSpaceURI.Length());

  return *aNameSpaceURI ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP
sdnDocAccessible::put_alternateViewMediaTypes(
    BSTR __RPC_FAR* aCommaSeparatedMediaTypes) {
  if (!aCommaSeparatedMediaTypes) return E_INVALIDARG;
  *aCommaSeparatedMediaTypes = nullptr;

  return !mMsaa->DocAcc() ? CO_E_OBJNOTCONNECTED : E_NOTIMPL;
}
