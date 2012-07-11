/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _ACCESSIBLE_HYPERTEXT_H
#define _ACCESSIBLE_HYPERTEXT_H

#include "nsISupports.h"

#include "ia2AccessibleText.h"
#include "AccessibleHypertext.h"

class ia2AccessibleHypertext : public ia2AccessibleText,
                               public IAccessibleHypertext
{
public:

  // IAccessibleText
  FORWARD_IACCESSIBLETEXT(ia2AccessibleText)

  // IAccessibleHypertext
  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_nHyperlinks(
      /* [retval][out] */ long* hyperlinkCount);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_hyperlink(
      /* [in] */ long index,
      /* [retval][out] */ IAccessibleHyperlink** hyperlink);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_hyperlinkIndex(
      /* [in] */ long charIndex,
      /* [retval][out] */ long* hyperlinkIndex);

  // nsISupports
  NS_IMETHOD QueryInterface(const nsIID& uuid, void** result) = 0;
};

#endif

