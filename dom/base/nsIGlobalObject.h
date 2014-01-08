/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIGlobalObject_h__
#define nsIGlobalObject_h__

#include "nsISupports.h"
#include "js/TypeDecls.h"

#define NS_IGLOBALOBJECT_IID \
{ 0xe2538ded, 0x13ef, 0x4f4d, \
{ 0x94, 0x6b, 0x65, 0xd3, 0x33, 0xb4, 0xf0, 0x3c } }

class nsIPrincipal;

class nsIGlobalObject : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IGLOBALOBJECT_IID)

  virtual JSObject* GetGlobalJSObject() = 0;

  // This method is not meant to be overridden.
  nsIPrincipal* PrincipalOrNull();
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIGlobalObject,
                              NS_IGLOBALOBJECT_IID)

#endif // nsIGlobalObject_h__
