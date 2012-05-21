/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _ACCESSIBLE_VALUE_H
#define _ACCESSIBLE_VALUE_H

#include "nsISupports.h"

#include "AccessibleValue.h"

class CAccessibleValue: public IAccessibleValue
{
public:

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID, void**);

  // IAccessibleValue
  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_currentValue(
      /* [retval][out] */ VARIANT *currentValue);

  virtual HRESULT STDMETHODCALLTYPE setCurrentValue(
      /* [in] */ VARIANT value);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_maximumValue(
      /* [retval][out] */ VARIANT *maximumValue);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_minimumValue(
      /* [retval][out] */ VARIANT *minimumValue);

  // nsISupports
  NS_IMETHOD QueryInterface(const nsIID& uuid, void** result) = 0;

};

#endif

