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

/* All the XPConnect public interfaces. */

#ifndef nsIXPConnect_h___
#define nsIXPConnect_h___

// XXX for now...
#include "nsISupports.h"
#include "jsapi.h"

/*
 * The linkage of XPC API functions differs depending on whether the file is
 * used within the XPC library or not.  Any source file within the XPC
 * library should define EXPORT_XPC_API whereas any client of the library
 * should not.
 */
#ifdef EXPORT_XPC_API
#define XPC_PUBLIC_API(t)    JS_EXPORT_API(t)
#define XPC_PUBLIC_DATA(t)   JS_EXPORT_DATA(t)
#else
#define XPC_PUBLIC_API(t)    JS_IMPORT_API(t)
#define XPC_PUBLIC_DATA(t)   JS_IMPORT_DATA(t)
#endif

#define XPC_FRIEND_API(t)    XPC_PUBLIC_API(t)
#define XPC_FRIEND_DATA(t)   XPC_PUBLIC_DATA(t)

// XXX break these up into separate files...
// XXX declare them in XPIDL :)

/***************************************************************************/
// {215DBE00-94A7-11d2-BA58-00805F8A5DD7}
#define NS_IJSCONTEXT_IID     \
{ 0x215dbe00, 0x94a7, 0x11d2, \
  { 0xba, 0x58, 0x0, 0x80, 0x5f, 0x8a, 0x5d, 0xd7 } }

class nsIJSContext : public nsISupports
{
public:
    NS_IMETHOD GetNative(JSContext** cx) = 0;
};

/***************************************************************************/
// {215DBE01-94A7-11d2-BA58-00805F8A5DD7}
#define NS_IJSOBJECT_IID        \
{ 0x215dbe01, 0x94a7, 0x11d2,   \
  { 0xba, 0x58, 0x0, 0x80, 0x5f, 0x8a, 0x5d, 0xd7 } }

class nsIJSObject : public nsISupports
{
public:
    NS_IMETHOD GetNative(JSObject** jsobj) = 0;
};

/***************************************************************************/
// {215DBE02-94A7-11d2-BA58-00805F8A5DD7}
#define NS_IXPCONNECT_WRAPPED_NATIVE_IID   \
{ 0x215dbe02, 0x94a7, 0x11d2,               \
  { 0xba, 0x58, 0x0, 0x80, 0x5f, 0x8a, 0x5d, 0xd7 } }

class nsIXPConnectWrappedNative : public nsISupports
{
    // no methods!
};

/***************************************************************************/
// {215DBE03-94A7-11d2-BA58-00805F8A5DD7}
#define NS_IXPCONNECT_WRAPPED_JS_IID   \
{ 0x215dbe03, 0x94a7, 0x11d2,           \
  { 0xba, 0x58, 0x0, 0x80, 0x5f, 0x8a, 0x5d, 0xd7 } }

class nsIXPConnectWrappedJS : public nsISupports
{
    // no methods!
};

/***************************************************************************/
// forward declarations of 2 non-XPCOM classes...
class nsXPCMethodInfo;
class nsXPCConstant;

// {215DBE04-94A7-11d2-BA58-00805F8A5DD7}
#define NS_IINTERFACEINFO_IID   \
{ 0x215dbe04, 0x94a7, 0x11d2,   \
  { 0xba, 0x58, 0x0, 0x80, 0x5f, 0x8a, 0x5d, 0xd7 } }

class nsIInterfaceInfo : public nsISupports
{
public:
    NS_IMETHOD GetName(const char** name) = 0;
    NS_IMETHOD GetIID(const nsIID** iid) = 0;

    NS_IMETHOD GetParent(nsIInterfaceInfo** parent) = 0;

    // these include counts of parents
    NS_IMETHOD GetMethodCount(int* count) = 0;
    NS_IMETHOD GetConstantCount(int* count) = 0;

    // these include methods and constants of parents
    NS_IMETHOD GetMethodInfo(unsigned index, const nsXPCMethodInfo** info) = 0;
    NS_IMETHOD GetConstant(unsigned index, const nsXPCConstant** constant) = 0;
};

/***************************************************************************/
// {EFAE37B0-946D-11d2-BA58-00805F8A5DD7}
#define NS_IXPCONNECT_IID    \
{ 0xefae37b0, 0x946d, 0x11d2, \
  {0xba, 0x58, 0x0, 0x80, 0x5f, 0x8a, 0x5d, 0xd7} }

class nsIXPConnect : public nsISupports
{
public:

    NS_IMETHOD InitJSContext(nsIJSContext* aJSContext,
                             nsIJSObject* aGlobalJSObj) = 0;
    
    NS_IMETHOD GetInterfaceInfo(REFNSIID aIID,
                                nsIInterfaceInfo** info) = 0;

    NS_IMETHOD WrapNative(nsIJSContext* aJSContext,
                          nsISupports* aCOMObj,
                          REFNSIID aIID,
                          nsIXPConnectWrappedNative** aWrapper) = 0;

    NS_IMETHOD WrapJS(nsIJSContext* aJSContext,
                      nsIJSObject* aJSObj,
                      REFNSIID aIID,
                      nsIXPConnectWrappedJS** aWrapper) = 0;

    NS_IMETHOD GetJSObjectOfWrappedJS(nsIXPConnectWrappedJS* aWrapper,
                                      nsIJSObject** aJSObj) = 0;

    NS_IMETHOD GetJSObjectOfWrappedNative(nsIXPConnectWrappedNative* aWrapper,
                                          nsIJSObject** aJSObj) = 0;

    NS_IMETHOD GetNativeOfWrappedNative(nsIXPConnectWrappedNative* aWrapper,
                                        nsISupports** aObj) = 0;

    // other stuff...

};

JS_BEGIN_EXTERN_C

XPC_PUBLIC_API(nsIXPConnect*)
XPC_GetXPConnect();

XPC_PUBLIC_API(nsIJSContext*)
XPC_NewJSContext(JSContext* cx);

XPC_PUBLIC_API(nsIJSObject*)
XPC_NewJSObject(nsIJSContext* aJSContext, JSObject* jsobj);

JS_END_EXTERN_C

#endif /* nsIXPConnect_h___ */
