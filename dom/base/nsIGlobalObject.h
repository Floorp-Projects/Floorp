/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIGlobalObject_h__
#define nsIGlobalObject_h__

#include "nsISupports.h"
#include "nsIScriptObjectPrincipal.h"

class JSObject;

#define NS_IGLOBALOBJECT_IID \
{ 0x8503e9a9, 0x530, 0x4b26,  \
{ 0xae, 0x24, 0x18, 0xca, 0x38, 0xe5, 0xed, 0x17 } }

class nsIGlobalObject : public nsIScriptObjectPrincipal
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IGLOBALOBJECT_IID)

  virtual JSObject* GetGlobalJSObject() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIGlobalObject,
                              NS_IGLOBALOBJECT_IID)

#endif // nsIGlobalObject_h__
