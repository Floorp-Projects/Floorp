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

/* All the XPConnect private declarations - only include locally. */

#ifndef xpcprivate_h___
#define xpcprivate_h___

#include <string.h>
#include <stdlib.h>
#include "nscore.h"
#include "nsISupports.h"
#include "nsIXPConnect.h"
#include "jsapi.h"
#include "jshash.h"
#include "xpcforwards.h"

#include "xpcbogusjs.h"
#include "xpcbogusii.h"


/***************************************************************************/

class nsXPConnect : public nsIXPConnect
{
    // all the interface method declarations...
    NS_DECL_ISUPPORTS;

    NS_IMETHOD InitJSContext(nsIJSContext* aJSContext,
                             nsIJSObject* aGlobalJSObj);

    NS_IMETHOD GetInterfaceInfo(REFNSIID aIID,
                                nsIInterfaceInfo** info);

    NS_IMETHOD WrapNative(nsIJSContext* aJSContext,
                          nsISupports* aCOMObj,
                          REFNSIID aIID,
                          nsIXPConnectWrappedNative** aWrapper);

    NS_IMETHOD WrapJS(nsIJSContext* aJSContext,
                      nsIJSObject* aJSObj,
                      REFNSIID aIID,
                      nsIXPConnectWrappedJS** aWrapper);

    NS_IMETHOD GetJSObjectOfWrappedJS(nsIXPConnectWrappedJS* aWrapper,
                                      nsIJSObject** aJSObj);

    NS_IMETHOD GetJSObjectOfWrappedNative(nsIXPConnectWrappedNative* aWrapper,
                                          nsIJSObject** aJSObj);

    NS_IMETHOD GetNativeOfWrappedNative(nsIXPConnectWrappedNative* aWrapper,
                                        nsISupports** aObj);


    /// non-interface implementation
public:
    static nsXPConnect* GetXPConnect();

    JSContext2XPCContextMap* GetContextMap() {return mContextMap;}
    XPCContext*              GetContext(JSContext* cx);

    virtual ~nsXPConnect();
private:
    nsXPConnect();
    XPCContext*  NewContext(JSContext* cx, JSObject* global);

private:
    JSContext2XPCContextMap* mContextMap;
    static nsXPConnect*           mSelf;
};

/*************************/
// XPCContext is mostly a dumb class to hold JSContext specific data and
// maps that let us find wrappers created for the given JSContext.

// no virtuals
class XPCContext
{
public:
    static XPCContext* newXPCContext(JSContext* aJSContext,
                                   JSObject* aGlobalObj,
                                   int WrappedJSMapSize,
                                   int WrappedNativeMapSize,
                                   int WrappedJSClassMapSize,
                                   int WrappedNativeClassMapSize);

    JSContext*      GetJSContext()      {return mJSContext;}
    JSObject*       GetGlobalObject()   {return mGlobalObj;}

    JSObject2WrappedJSMap*     GetWrappedJSMap()          {return mWrappedJSMap;}
    Native2WrappedNativeMap*   GetWrappedNativeMap()      {return mWrappedNativeMap;}
    IID2WrappedJSClassMap*     GetWrappedJSClassMap()     {return mWrappedJSClassMap;}
    IID2WrappedNativeClassMap* GetWrappedNativeClassMap() {return mWrappedNativeClassMap;}

    ~XPCContext();
private:
    XPCContext();    // no implementation
    XPCContext(JSContext* aJSContext,
              JSObject* aGlobalObj,
              int WrappedJSMapSize,
              int WrappedNativeMapSize,
              int WrappedJSClassMapSize,
              int WrappedNativeClassMapSize);
private:
    JSContext*  mJSContext;
    JSObject*   mGlobalObj;
    JSObject2WrappedJSMap* mWrappedJSMap;
    Native2WrappedNativeMap* mWrappedNativeMap;
    IID2WrappedJSClassMap* mWrappedJSClassMap;
    IID2WrappedNativeClassMap* mWrappedNativeClassMap;
};

/*************************/

class nsXPCWrappedJS : public nsIXPConnectWrappedJS
{
    // our vtbl stubs here...

    // XXX implement...
public:
    JSObject* GetJSObject(){return NULL;}
    nsXPCWrappedJSClass*  GetClass() {return NULL;}

};

class nsXPCWrappedJSClass : public nsISupports
{
    // XXX implement...
public:

    REFNSIID GetIID(){return mIID;}
    XPCContext*  GetXPCContext() {return mXPCContext;}

private:
    XPCContext* mXPCContext;
    nsIID mIID;

};

/*************************/

// nsXPCWrappedNativeClass maintains an array of these things
struct XPCNativeMemberDescriptor
{
    enum MemberCategory{
        CONSTANT,
        METHOD,
        ATTRIB_RO,
        ATTRIB_RW
    };

    JSObject*       invokeFuncObj;
    jsid            id;  /* hashed name for quick JS property lookup */
    uintN           index; /* in InterfaceInfo for const, method, and get */
    uintN           index2; /* in InterfaceInfo for set */
    MemberCategory  category;
    uintN           maxParamCount;
    uintN           maxScratchWordCount;

    XPCNativeMemberDescriptor()
        : invokeFuncObj(NULL), id(0),
          maxParamCount(-1),
          maxScratchWordCount(-1){}
};

// this interfaces exists just so we can refcount nsXPCWrappedNativeClass
// {C9E36280-954A-11d2-BA5A-00805F8A5DD7}
#define NS_IXPCONNECT_WRAPPED_NATIVE_CLASS_IID \
{ 0xc9e36280, 0x954a, 0x11d2,                   \
  { 0xba, 0x5a, 0x0, 0x80, 0x5f, 0x8a, 0x5d, 0xd7 } }

class nsIXPCWrappedNativeClass : public nsISupports
{
    // no methods
};

class nsXPCWrappedNativeClass : public nsIXPCWrappedNativeClass
{
    // all the interface method declarations...
    NS_DECL_ISUPPORTS;

public:
    static nsXPCWrappedNativeClass* GetNewOrUsedClass(XPCContext* xpcc,
                                                     REFNSIID aIID);

    REFNSIID GetIID() const {return mIID;}
    nsIInterfaceInfo* GetInterfaceInfo() const {return mInfo;}
    XPCContext*  GetXPCContext() const {return mXPCContext;}

    static JSBool InitForContext(XPCContext* xpcc);
    JSObject* NewInstanceJSObject(nsXPCWrappedNative* self);

    int GetMemberCount() const {return mMemberCount;}
    const XPCNativeMemberDescriptor* GetMemberDescriptor(int i) const
    {
        NS_PRECONDITION(i >= 0 && i < mMemberCount,"bad index");
        return &mMembers[i];
    }

    const XPCNativeMemberDescriptor* LookupMemberByID(jsid id) const;

    JSBool GetConstantAsJSVal(nsXPCWrappedNative* wrapper,
                              const XPCNativeMemberDescriptor* desc,
                              jsval* vp);

    JSBool CallWrappedMethod(nsXPCWrappedNative* wrapper,
                             const XPCNativeMemberDescriptor* desc,
                             JSBool isAttributeSet,
                             uintN argc, jsval *argv, jsval *vp);

    JSBool GetAttributeAsJSVal(nsXPCWrappedNative* wrapper,
                               const XPCNativeMemberDescriptor* desc,
                               jsval* vp)
    {
        return CallWrappedMethod(wrapper, desc, JS_FALSE, 0, NULL, vp);
    }

    JSBool SetAttributeFromJSVal(nsXPCWrappedNative* wrapper,
                                 const XPCNativeMemberDescriptor* desc,
                                 jsval* vp)
    {
        return CallWrappedMethod(wrapper, desc, JS_TRUE, 1, vp, NULL);
    }

    JSObject* GetInvokeFunObj(const XPCNativeMemberDescriptor* desc);

    ~nsXPCWrappedNativeClass();
private:
    nsXPCWrappedNativeClass();   // not implemented
    nsXPCWrappedNativeClass(XPCContext* xpcc, REFNSIID aIID,
                           nsIInterfaceInfo* aInfo);

    const char* GetMethodName(int MethodIndex) const;
    JSContext* GetJSContext() {return mXPCContext->GetJSContext();}

    uintN GetMaxParamCount(const XPCNativeMemberDescriptor* desc)
    {
        if(-1 == desc->maxParamCount)
            SetDescriptorCounts(NS_CONST_CAST(XPCNativeMemberDescriptor*,desc));
        return desc->maxParamCount;
    }

    uintN GetMaxScratchWordCount(const XPCNativeMemberDescriptor* desc)
    {
        if(-1 == desc->maxScratchWordCount)
            SetDescriptorCounts(NS_CONST_CAST(XPCNativeMemberDescriptor*,desc));
        return desc->maxScratchWordCount;
    }

    void SetDescriptorCounts(XPCNativeMemberDescriptor* desc);
    void ReportError(const XPCNativeMemberDescriptor* desc, const char* msg);
    JSBool BuildMemberDescriptors();
    void  DestroyMemberDescriptors();

private:
    XPCContext* mXPCContext;
    nsIID mIID;
    nsIInterfaceInfo* mInfo;
    int mMemberCount;
    XPCNativeMemberDescriptor* mMembers;
};

class nsXPCWrappedNative : public nsIXPConnectWrappedNative
{
    // all the interface method declarations...
    NS_DECL_ISUPPORTS;

public:
    static nsXPCWrappedNative* GetNewOrUsedWrapper(XPCContext* xpcc,
                                                  nsISupports* aObj,
                                                  REFNSIID aIID);
    nsISupports* GetNative() {return mObj;}
    JSObject* GetJSObject() {return mJSObj;}
    nsXPCWrappedNativeClass* GetClass() {return mClass;}
    void JSObjectFinalized();

    virtual ~nsXPCWrappedNative();
private:
    nsXPCWrappedNative();    // not implemented
    nsXPCWrappedNative(nsISupports* aObj,
                      nsXPCWrappedNativeClass* aClass,
                      nsXPCWrappedNative* root);

    nsXPCWrappedNative* Find(REFNSIID aIID);

private:
    nsISupports* mObj;
    JSObject* mJSObj;
    nsXPCWrappedNativeClass* mClass;
    nsXPCWrappedNative* mRoot;
    nsXPCWrappedNative* mNext;
};

/************************************************/

// platform specific method invoker
nsresult
xpc_InvokeNativeMethod(void* that, PRUint32 index,
                       uint32 paramCount, nsXPCVarient* params);

/***************************************************************************/

// the include of declarations of the maps comes last because they have
// inlines which call methods on classes above.

#include "xpcmaps.h"

#endif /* xpcprivate_h___ */
