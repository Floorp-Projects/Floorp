/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _ACCESSIBLE_HYPERLINK_H
#define _ACCESSIBLE_HYPERLINK_H

#include "nsISupports.h"

#include "ia2AccessibleAction.h"
#include "AccessibleHyperlink.h"

class CAccessibleHyperlink: public ia2AccessibleAction,
                            public IAccessibleHyperlink
{
public:

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID, void**);

  // IAccessibleAction
  FORWARD_IACCESSIBLEACTION(ia2AccessibleAction)

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_anchor(
      /* [in] */ long index,
      /* [retval][out] */ VARIANT *anchor);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_anchorTarget(
      /* [in] */ long index,
      /* [retval][out] */ VARIANT *anchorTarget);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_startIndex(
      /* [retval][out] */ long *index);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_endIndex(
      /* [retval][out] */ long *index);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_valid(
      /* [retval][out] */ boolean *valid);

  // nsISupports
  NS_IMETHOD QueryInterface(const nsIID& uuid, void** result) = 0;

};

#endif

