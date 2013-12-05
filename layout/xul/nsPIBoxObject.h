/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPIBoxObject_h___
#define nsPIBoxObject_h___

#include "nsIBoxObject.h"

// {2b8bb262-1b0f-4572-ba87-5d4ae4954445}
#define NS_PIBOXOBJECT_IID \
{ 0x2b8bb262, 0x1b0f, 0x4572, \
  { 0xba, 0x87, 0x5d, 0x4a, 0xe4, 0x95, 0x44, 0x45 } }


class nsIContent;

class nsPIBoxObject : public nsIBoxObject
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_PIBOXOBJECT_IID)

  virtual nsresult Init(nsIContent* aContent) = 0;

  // Drop the weak ref to the content node as needed
  virtual void Clear() = 0;

  // The values cached by the implementation of this interface should be
  // cleared when this method is called.
  virtual void ClearCachedValues() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsPIBoxObject, NS_PIBOXOBJECT_IID)

#endif

