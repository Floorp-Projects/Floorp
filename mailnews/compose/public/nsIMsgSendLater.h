/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef __nsIMsgSendLater_h__
#define __nsIMsgSendLater_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsIMsgIdentity.h" /* interface nsIMsgCompFields */
#include "nsID.h" /* interface nsID */
#include "rosetta.h"

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif

/* starting interface:    nsIMsgSendLater */

// {E15C83E8-1CF4-11d3-8EF0-00A024A7D144}
#define NS_IMSGSENDLATER_IID_STR "E15C83E8-1CF4-11d3-8EF0-00A024A7D144"
#define NS_IMSGSENDLATER_IID \
    { 0xe15c83e8, 0x1cf4, 0x11d3, \
    { 0x8e, 0xf0, 0x0, 0xa0, 0x24, 0xa7, 0xd1, 0x44 } };

class nsIMsgSendLater : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IMSGSENDLATER_IID)

  NS_IMETHOD  SendUnsentMessages(nsIMsgIdentity *identity) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIMsgSendLater *priv);
#endif
};

#endif /* __nsIMsgSendLater_h__ */
