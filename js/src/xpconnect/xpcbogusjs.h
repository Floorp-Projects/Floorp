/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/* Temporary JSAPI interface related stuff. */

#ifndef xpcbogusjs_h___
#define xpcbogusjs_h___

class nsJSContext : public nsIJSContext
{
    NS_DECL_ISUPPORTS;
    NS_IMETHOD GetNative(JSContext** cx);

    // implementation...
    nsJSContext(JSContext* cx);
private:
    nsJSContext();  // no implementation
private:
    JSContext*  mJSContext;
};

/***************************************************************************/

class nsJSObject : public nsIJSObject
{
    NS_DECL_ISUPPORTS;
    NS_IMETHOD GetNative(JSObject** jsobj);

    // implementation...
    nsJSObject(nsIJSContext* aJSContext, JSObject* jsobj);
    ~nsJSObject();
private:
    nsJSObject();  // no implementation
private:
    nsIJSContext* mJSContext;
    JSObject*  mJSObject;
};

#endif /* xpcbogusjs_h___ */
