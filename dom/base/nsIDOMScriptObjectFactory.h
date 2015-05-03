/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIDOMScriptObjectFactory_h__
#define nsIDOMScriptObjectFactory_h__

#include "nsISupports.h"
#include "nsIDOMClassInfo.h"
#include "nsString.h"

#define NS_IDOM_SCRIPT_OBJECT_FACTORY_IID \
{ 0x2a50e17c, 0x46ff, 0x4150, \
  { 0xbb, 0x46, 0xd8, 0x07, 0xb3, 0x36, 0xde, 0xab } }

typedef nsXPCClassInfo* (*nsDOMClassInfoExternalConstructorFnc)
  (const char* aName);

class nsIDOMScriptObjectFactory : public nsISupports {
public:  
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IDOM_SCRIPT_OBJECT_FACTORY_IID)

  NS_IMETHOD_(nsISupports *) GetClassInfoInstance(nsDOMClassInfoID aID) = 0;
  NS_IMETHOD_(nsISupports *) GetExternalClassInfoInstance(const nsAString& aName) = 0;

  // Register the info for an external class. aName must be static
  // data, it will not be deleted by the DOM code. aProtoChainInterface
  // must be registered in the JAVASCRIPT_DOM_INTERFACE category, or
  // prototypes for this class won't work (except if the interface
  // name starts with nsIDOM).
  NS_IMETHOD RegisterDOMClassInfo(const char *aName,
                                  nsDOMClassInfoExternalConstructorFnc aConstructorFptr,
                                  const nsIID *aProtoChainInterface,
                                  const nsIID **aInterfaces,
                                  uint32_t aScriptableFlags,
                                  bool aHasClassInterface,
                                  const nsCID *aConstructorCID) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIDOMScriptObjectFactory,
                              NS_IDOM_SCRIPT_OBJECT_FACTORY_IID)

#endif /* nsIDOMScriptObjectFactory_h__ */
