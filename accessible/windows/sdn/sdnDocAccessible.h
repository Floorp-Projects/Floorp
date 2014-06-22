/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_sdnDocAccessible_h_
#define mozilla_a11y_sdnDocAccessible_h_

#include "ISimpleDOMDocument.h"
#include "IUnknownImpl.h"

#include "DocAccessibleWrap.h"

namespace mozilla {
namespace a11y {

class sdnDocAccessible MOZ_FINAL : public ISimpleDOMDocument
{
public:
  sdnDocAccessible(DocAccessibleWrap* aAccessible) : mAccessible(aAccessible) {};
  ~sdnDocAccessible() { };

  DECL_IUNKNOWN

  // ISimpleDOMDocument
  virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_URL(
    /* [out] */ BSTR __RPC_FAR *url);

  virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_title(
    /* [out] */ BSTR __RPC_FAR *title);

  virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_mimeType(
    /* [out] */ BSTR __RPC_FAR *mimeType);

  virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_docType(
    /* [out] */ BSTR __RPC_FAR *docType);

  virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_nameSpaceURIForID(
    /* [in] */ short nameSpaceID,
    /* [out] */ BSTR __RPC_FAR *nameSpaceURI);

  virtual /* [id] */ HRESULT STDMETHODCALLTYPE put_alternateViewMediaTypes(
    /* [in] */ BSTR __RPC_FAR *commaSeparatedMediaTypes);

protected:
  nsRefPtr<DocAccessibleWrap> mAccessible;
};

} // namespace a11y
} // namespace mozilla

#endif
