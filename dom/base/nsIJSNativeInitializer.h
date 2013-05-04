/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIJSNativeInitializer_h__
#define nsIJSNativeInitializer_h__

#include "nsISupports.h"
#include "jsapi.h"

#define NS_IJSNATIVEINITIALIZER_IID \
{ 0xdb48eee5, 0x89a4, 0x4f18,       \
  { 0x86, 0xd0, 0x4c, 0x4e, 0x9d, 0x4b, 0xf8, 0x7e } }


/**
 * A JavaScript specific interface used to initialize new
 * native objects, created as a result of calling a
 * JavaScript constructor. The arguments are passed in
 * their raw form as JS::Value's.
 */

class nsIJSNativeInitializer : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IJSNATIVEINITIALIZER_IID)

  /**
   * Initialize a newly created native instance using the owner of the
   * constructor and the parameters passed into the JavaScript constructor.
   */
  NS_IMETHOD Initialize(nsISupports* aOwner, JSContext *cx, JSObject *obj,
                        const JS::CallArgs& args) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIJSNativeInitializer,
                              NS_IJSNATIVEINITIALIZER_IID)

#endif // nsIJSNativeInitializer_h__
