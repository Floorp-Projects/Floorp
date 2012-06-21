/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _ACCESSIBLE_EDITABLETEXT_H
#define _ACCESSIBLE_EDITABLETEXT_H

#include "nsISupports.h"
#include "nsIAccessibleEditableText.h"

#include "AccessibleEditableText.h"

class ia2AccessibleEditableText: public IAccessibleEditableText
{
public:

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID, void**);

  // IAccessibleEditableText
  virtual HRESULT STDMETHODCALLTYPE copyText(
      /* [in] */ long startOffset,
      /* [in] */ long endOffset);

  virtual HRESULT STDMETHODCALLTYPE deleteText(
      /* [in] */ long startOffset,
      /* [in] */ long endOffset);

  virtual HRESULT STDMETHODCALLTYPE insertText(
      /* [in] */ long offset,
      /* [in] */ BSTR *text);

  virtual HRESULT STDMETHODCALLTYPE cutText(
      /* [in] */ long startOffset,
      /* [in] */ long endOffset);

  virtual HRESULT STDMETHODCALLTYPE pasteText(
      /* [in] */ long offset);

  virtual HRESULT STDMETHODCALLTYPE replaceText(
      /* [in] */ long startOffset,
      /* [in] */ long endOffset,
      /* [in] */ BSTR *text);

  virtual HRESULT STDMETHODCALLTYPE setAttributes(
      /* [in] */ long startOffset,
      /* [in] */ long endOffset,
      /* [in] */ BSTR *attributes);

  // nsISupports
  NS_IMETHOD QueryInterface(const nsIID& uuid, void** result) = 0;
};

#endif

