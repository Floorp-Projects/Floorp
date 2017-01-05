/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPIListBoxObject_h__
#define nsPIListBoxObject_h__

class nsListBoxBodyFrame;

// fa9549f7-ee09-48fc-89f7-30cceee21c15
#define NS_PILISTBOXOBJECT_IID \
{ 0xfa9549f7, 0xee09, 0x48fc, \
  { 0x89, 0xf7, 0x30, 0xcc, 0xee, 0xe2, 0x1c, 0x15 } }

#include "nsIListBoxObject.h"

class nsPIListBoxObject : public nsIListBoxObject {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_PILISTBOXOBJECT_IID)
  /**
   * Get the list box body.  This will search for it as needed.
   * If aFlush is false we don't FlushType::Frames though.
   */
  virtual nsListBoxBodyFrame* GetListBoxBody(bool aFlush) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsPIListBoxObject, NS_PILISTBOXOBJECT_IID)

#endif // nsPIListBoxObject_h__
