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

#include "nsISupports.h"
#include "jsapi.h"
#include "nsIInterfaceInfo.h"
#include "nsIInterfaceInfoManager.h"
#include "xptinfo.h"

#include "xpccomponents.h"
#include "xpcjsid.h"

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

// XXX break these up into separate files?
// XXX declare them in XPIDL :)

/***************************************************************************/
// forward declarations...
class nsIXPCScriptable;
class nsIInterfaceInfo;

// {215DBE02-94A7-11d2-BA58-00805F8A5DD7}
#define NS_IXPCONNECT_WRAPPED_NATIVE_IID   \
{ 0x215dbe02, 0x94a7, 0x11d2,               \
  { 0xba, 0x58, 0x0, 0x80, 0x5f, 0x8a, 0x5d, 0xd7 } }

class nsIXPConnectWrappedNative : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IXPCONNECT_WRAPPED_NATIVE_IID)

    NS_IMETHOD GetDynamicScriptable(nsIXPCScriptable** p) = 0;
    NS_IMETHOD GetArbitraryScriptable(nsIXPCScriptable** p) = 0;
    NS_IMETHOD GetJSObject(JSObject** aJSObj) = 0;
    NS_IMETHOD GetNative(nsISupports** aObj) = 0;
    NS_IMETHOD GetInterfaceInfo(nsIInterfaceInfo** info) = 0;
    NS_IMETHOD GetIID(nsIID** iid) = 0; // returns IAllocatator alloc'd copy
    NS_IMETHOD DebugDump(int depth) = 0;
    // XXX other methods?
};

/***************************************************************************/
// {215DBE03-94A7-11d2-BA58-00805F8A5DD7}
#define NS_IXPCONNECT_WRAPPED_JS_IID   \
{ 0x215dbe03, 0x94a7, 0x11d2,           \
  { 0xba, 0x58, 0x0, 0x80, 0x5f, 0x8a, 0x5d, 0xd7 } }

class nsIXPConnectWrappedJS : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IXPCONNECT_WRAPPED_JS_IID)
    // no methods allowed since this has a shared vtbl!
    //
    // To manipulate this wrapper (as opposed to manipulating the wrapped
    // JSObject via this wrapper) do a QueryInterface for the
    // nsIXPConnectWrappedJSMethods interface 
    // i.e. 'nsIXPConnectWrappedJSMethods::GetIID()'
    // and use the methods on that interface. (see below)
};

/******************************************/

// {BED52030-BCA6-11d2-BA79-00805F8A5DD7}
#define NS_IXPCONNECT_WRAPPED_JS_METHODS_IID   \
{ 0xbed52030, 0xbca6, 0x11d2, \
  { 0xba, 0x79, 0x0, 0x80, 0x5f, 0x8a, 0x5d, 0xd7 } }

class nsIXPConnectWrappedJSMethods : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IXPCONNECT_WRAPPED_JS_METHODS_IID)

    NS_IMETHOD GetJSObject(JSObject** aJSObj) = 0;
    NS_IMETHOD GetInterfaceInfo(nsIInterfaceInfo** info) = 0;
    NS_IMETHOD GetIID(nsIID** iid) = 0; // returns IAllocatator alloc'd copy
    NS_IMETHOD DebugDump(int depth) = 0;
    // XXX other methods?
};

/***************************************************************************/
// {EFAE37B0-946D-11d2-BA58-00805F8A5DD7}
#define NS_IXPCONNECT_IID    \
{ 0xefae37b0, 0x946d, 0x11d2, \
  {0xba, 0x58, 0x0, 0x80, 0x5f, 0x8a, 0x5d, 0xd7} }

class nsIXPConnect : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IXPCONNECT_IID)

    NS_IMETHOD InitJSContext(JSContext* aJSContext,
                             JSObject* aGlobalJSObj) = 0;

    NS_IMETHOD InitJSContextWithNewWrappedGlobal(JSContext* aJSContext,
                          nsISupports* aCOMObj,
                          REFNSIID aIID,
                          nsIXPConnectWrappedNative** aWrapper) = 0;

    // XXX add 'AbandonJSContext' method and all that implies?

    NS_IMETHOD WrapNative(JSContext* aJSContext,
                          nsISupports* aCOMObj,
                          REFNSIID aIID,
                          nsIXPConnectWrappedNative** aWrapper) = 0;

    NS_IMETHOD WrapJS(JSContext* aJSContext,
                      JSObject* aJSObj,
                      REFNSIID aIID,
                      nsIXPConnectWrappedJS** aWrapper) = 0;

    NS_IMETHOD GetWrappedNativeOfJSObject(JSContext* aJSContext,
                                    JSObject* aJSObj,
                                    nsIXPConnectWrappedNative** aWrapper) = 0;

    NS_IMETHOD DebugDump(int depth) = 0;
    // XXX other methods?
};

JS_BEGIN_EXTERN_C
// XXX Add support for this to be a service?
XPC_PUBLIC_API(nsIXPConnect*)
XPC_GetXPConnect();

XPC_PUBLIC_API(nsIXPCComponents*)
XPC_GetXPConnectComponentsObject();

#ifdef DEBUG
XPC_PUBLIC_API(void)
XPC_Dump(nsISupports* p, int depth);
#define XPC_DUMP(x,d) XPC_Dump(x,d)
#else
#define XPC_DUMP(x,d) ((void)0)
#endif

JS_END_EXTERN_C

#ifdef DEBUG
JS_BEGIN_EXTERN_C
// XXX temporary forward declaration
struct nsXPCVariant;
XPC_PUBLIC_API(nsresult)
XPC_TestInvoke(void* that, PRUint32 index,
               uint32 paramCount, nsXPCVariant* params);
JS_END_EXTERN_C
#endif

#endif /* nsIXPConnect_h___ */
