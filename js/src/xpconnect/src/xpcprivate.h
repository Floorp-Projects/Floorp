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
#include <stdarg.h>
#include "nscore.h"
#include "nsISupports.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsISupportsPrimitives.h"
#include "nsIGenericFactory.h"
#include "nsIAllocator.h"
#include "nsIXPConnect.h"
#include "nsIInterfaceInfo.h"
#include "nsIInterfaceInfoManager.h"
#include "nsIXPCScriptable.h"
#include "nsIXPCSecurityManager.h"
#include "xptcall.h"
#include "jsapi.h"
#include "jshash.h"
#include "jsprf.h"
#include "jsinterp.h"
#include "jscntxt.h"
#include "jsdbgapi.h"
#include "xptinfo.h"
#include "xpcforwards.h"
#include "xpclog.h"
#include "xpccomponents.h"
#include "xpcexception.h"
#include "xpcjsid.h"
#include "prlong.h"

#include "nsIJSContextStack.h"
#include "prthread.h"
#include "nsDeque.h"

#include "nsIScriptObjectOwner.h"   // for DOM hack in xpcconvert.cpp
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"

extern const char XPC_VAL_STR[];        // 'value' property name for out params
extern const char XPC_COMPONENTS_STR[]; // 'Components' property name
extern const char XPC_ARG_FORMATTER_FORMAT_STR[]; // format string

/***************************************************************************/
// useful macros...

#define XPC_STRING_GETTER_BODY(dest, src) \
    NS_ENSURE_ARG_POINTER(dest); \
    char* result; \
    if(src) \
        result = (char*) nsAllocator::Clone(src, \
                                sizeof(char)*(strlen(src)+1)); \
    else \
        result = nsnull; \
    *dest = result; \
    return (result || !src) ? NS_OK : NS_ERROR_OUT_OF_MEMORY

/***************************************************************************/

class nsXPConnect : public nsIXPConnect
{
    // all the interface method declarations...
    NS_DECL_ISUPPORTS

    NS_IMETHOD InitJSContext(JSContext* aJSContext,
                             JSObject* aGlobalJSObj,
                             JSBool AddComponentsObject);

    NS_IMETHOD InitJSContextWithNewWrappedGlobal(JSContext* aJSContext,
                          nsISupports* aCOMObj,
                          REFNSIID aIID,
                          JSBool AddComponentsObject,
                          nsIXPConnectWrappedNative** aWrapper);

    NS_IMETHOD AddNewComponentsObject(JSContext* aJSContext,
                                      JSObject* aGlobalJSObj);

    NS_IMETHOD CreateComponentsObject(nsIXPCComponents** aComponentsObj);

    NS_IMETHOD WrapNative(JSContext* aJSContext,
                          nsISupports* aCOMObj,
                          REFNSIID aIID,
                          nsIXPConnectWrappedNative** aWrapper);

    NS_IMETHOD WrapJS(JSContext* aJSContext,
                      JSObject* aJSObj,
                      REFNSIID aIID,
                      nsISupports** aWrapper);

    NS_IMETHOD GetWrappedNativeOfJSObject(JSContext* aJSContext,
                                    JSObject* aJSObj,
                                    nsIXPConnectWrappedNative** aWrapper);

    NS_IMETHOD DebugDump(int depth);
    NS_IMETHOD DebugDumpObject(nsISupports* p, int depth);

    NS_IMETHOD AbandonJSContext(JSContext* aJSContext);

    NS_IMETHOD SetSecurityManagerForJSContext(JSContext* aJSContext,
                                    nsIXPCSecurityManager* aManager,
                                    PRUint16 flags);

    NS_IMETHOD GetSecurityManagerForJSContext(JSContext* aJSContext,
                                    nsIXPCSecurityManager** aManager,
                                    PRUint16* flags);

    NS_IMETHOD GetCurrentJSStack(nsIJSStackFrameLocation** aStack);

    NS_IMETHOD CreateStackFrameLocation(JSBool isJSFrame,
                                        const char* aFilename,
                                        const char* aFunctionName,
                                        PRInt32 aLineNumber,
                                        nsIJSStackFrameLocation* aCaller,
                                        nsIJSStackFrameLocation** aStack);

    NS_IMETHOD GetPendingException(nsIXPCException** aException);
    /* pass nsnull to clear pending exception */
    NS_IMETHOD SetPendingException(nsIXPCException* aException);

    // non-interface implementation
public:
    static nsXPConnect* GetXPConnect();
    static nsIInterfaceInfoManager* GetInterfaceInfoManager(nsXPConnect* xpc = NULL);
    static XPCContext*  GetContext(JSContext* cx, nsXPConnect* xpc = NULL);
    static XPCJSThrower* GetJSThrower(nsXPConnect* xpc = NULL);
    static JSBool IsISupportsDescendent(nsIInterfaceInfo* info);
    static nsIJSContextStack* GetContextStack(nsXPConnect* xpc = NULL);

    JSContext2XPCContextMap* GetContextMap() {return mContextMap;}
    nsIXPCScriptable* GetArbitraryScriptable() {return mArbitraryScriptable;}

    virtual ~nsXPConnect();
private:
    nsXPConnect();
    XPCContext*  NewContext(JSContext* cx, JSObject* global,
                            JSBool doInit = JS_TRUE);

private:
    static nsXPConnect* mSelf;
    JSContext2XPCContextMap* mContextMap;
    nsIXPCScriptable* mArbitraryScriptable;
    nsIInterfaceInfoManager* mInterfaceInfoManager;
    XPCJSThrower* mThrower;
    nsIJSContextStack* mContextStack;
};

/***************************************************************************/
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

    JSContext*      GetJSContext()      const {return mJSContext;}
    JSObject*       GetGlobalObject()   const {return mGlobalObj;}
    nsXPConnect*    GetXPConnect()      const {return mXPConnect;}

    JSObject2WrappedJSMap*     GetWrappedJSMap()          const
        {return mWrappedJSMap;}
    Native2WrappedNativeMap*   GetWrappedNativeMap()      const
        {return mWrappedNativeMap;}
    IID2WrappedJSClassMap*     GetWrappedJSClassMap()     const
        {return mWrappedJSClassMap;}
    IID2WrappedNativeClassMap* GetWrappedNativeClassMap() const
        {return mWrappedNativeClassMap;}

    // To add a new string: add to this list and to XPCContext::mStrings
    // at the top of xpccontext.cpp
    enum {
        IDX_CONSTRUCTOR     = 0 ,
        IDX_TO_STRING       ,
        IDX_LAST_RESULT     ,
        IDX_TOTAL_COUNT // just a count of the above
    };

    jsid GetStringID(uintN index) const
    {
        NS_ASSERTION(index < IDX_TOTAL_COUNT, "index out of range");
        return mStrIDs[index];
    }
    const char* GetStringName(uintN index) const
    {
        NS_ASSERTION(index < IDX_TOTAL_COUNT, "index out of range");
        return mStrings[index];
    }

    nsIXPCException* GetException()
        {
            NS_IF_ADDREF(mException);
            return mException;
        }
    void SetException(nsIXPCException* e)
        {
            NS_IF_ADDREF(e);
            NS_IF_RELEASE(mException);
            mException = e;
        }

    nsresult GetLastResult() {return mLastResult;}
    void SetLastResult(nsresult rc) {mLastResult = rc;}

    nsIXPCSecurityManager* GetSecurityManager() const
        {return mSecurityManager;}
    void SetSecurityManager(nsIXPCSecurityManager* aSecurityManager)
        {mSecurityManager = aSecurityManager;}

    PRUint16 GetSecurityManagerFlags() const
        {return mSecurityManagerFlags;}
    void SetSecurityManagerFlags(PRUint16 f)
        {mSecurityManagerFlags = f;}

    JSBool Init(JSObject* aGlobalObj = NULL);
    void DebugDump(int depth);

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
    static const char* mStrings[IDX_TOTAL_COUNT];
    nsXPConnect* mXPConnect;
    JSContext*  mJSContext;
    JSObject*   mGlobalObj;
    JSObject2WrappedJSMap* mWrappedJSMap;
    Native2WrappedNativeMap* mWrappedNativeMap;
    IID2WrappedJSClassMap* mWrappedJSClassMap;
    IID2WrappedNativeClassMap* mWrappedNativeClassMap;
    jsid mStrIDs[IDX_TOTAL_COUNT];
    nsresult mLastResult;
    nsIXPCSecurityManager* mSecurityManager;
    PRUint16 mSecurityManagerFlags;
    nsIXPCException* mException;
};

/***************************************************************************/
// code for throwing exceptions into JS

class XPCJSThrower
{
public:
    void ThrowBadResultException(nsresult rv,
                                 JSContext* cx,
                                 nsXPCWrappedNativeClass* clazz,
                                 const XPCNativeMemberDescriptor* desc,
                                 nsresult result);

    void ThrowBadParamException(nsresult rv,
                                JSContext* cx,
                                nsXPCWrappedNativeClass* clazz,
                                const XPCNativeMemberDescriptor* desc,
                                uintN paramNum);

    void ThrowException(nsresult rv,
                        JSContext* cx,
                        nsXPCWrappedNativeClass* clazz = nsnull,
                        const XPCNativeMemberDescriptor* desc = nsnull);

    XPCJSThrower(JSBool Verbose = JS_FALSE);
    ~XPCJSThrower();

private:
    void Verbosify(JSContext* cx,
                   nsXPCWrappedNativeClass* clazz,
                   const XPCNativeMemberDescriptor* desc,
                   char** psz, PRBool own);

    char* BuildCallerString(JSContext* cx);
    void BuildAndThrowException(JSContext* cx, nsresult rv, const char* sz);
    JSBool ThrowExceptionObject(JSContext* cx, nsIXPCException* e);

private:
    JSBool mVerbose;
};

/***************************************************************************/

// this interfaces exists so we can refcount nsXPCWrappedJSClass
// {2453EBA0-A9B8-11d2-BA64-00805F8A5DD7}
#define NS_IXPCONNECT_WRAPPED_JS_CLASS_IID  \
{ 0x2453eba0, 0xa9b8, 0x11d2,               \
  { 0xba, 0x64, 0x0, 0x80, 0x5f, 0x8a, 0x5d, 0xd7 } }

class nsIXPCWrappedJSClass : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IXPCONNECT_WRAPPED_JS_CLASS_IID)
    NS_IMETHOD DebugDump(int depth) = 0;
};

/*************************/

class nsXPCWrappedJSClass : public nsIXPCWrappedJSClass
{
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_IMETHOD DebugDump(int depth);
public:

    static nsXPCWrappedJSClass* GetNewOrUsedClass(XPCContext* xpcc,
                                                  REFNSIID aIID);
    REFNSIID GetIID() const {return mIID;}
    XPCContext*  GetXPCContext() const {return mXPCContext;}
    nsIInterfaceInfo* GetInterfaceInfo() const {return mInfo;}
    const char* GetInterfaceName();

    static JSBool InitForContext(XPCContext* xpcc);
    static JSBool IsWrappedJS(nsISupports* aPtr);

    NS_IMETHOD DelegatedQueryInterface(nsXPCWrappedJS* self, REFNSIID aIID,
                                       void** aInstancePtr);

    JSObject* GetRootJSObject(JSObject* aJSObj);

    NS_IMETHOD CallMethod(nsXPCWrappedJS* wrapper, uint16 methodIndex,
                        const nsXPTMethodInfo* info,
                        nsXPTCMiniVariant* params);

    void XPCContextBeingDestroyed();

    virtual ~nsXPCWrappedJSClass();
private:
    nsXPCWrappedJSClass();   // not implemented
    nsXPCWrappedJSClass(XPCContext* xpcc, REFNSIID aIID,
                        nsIInterfaceInfo* aInfo);

    JSContext* GetJSContext() const
        {return mXPCContext ? mXPCContext->GetJSContext() : NULL;}
    JSObject*  CreateIIDJSObject(REFNSIID aIID);
    JSObject*  NewOutObject();

    JSObject*  CallQueryInterfaceOnJSObject(JSObject* jsobj, REFNSIID aIID);

    JSBool IsReflectable(uint16 i) const
    {return (JSBool)(mDescriptors[i/32] & (1 << (i%32)));}
    void SetReflectable(uint16 i, JSBool b)
    {if(b) mDescriptors[i/32] |= (1 << (i%32));
     else mDescriptors[i/32] &= ~(1 << (i%32));}

private:
    XPCContext* mXPCContext;
    nsIInterfaceInfo* mInfo;
    char* mName;
    nsIID mIID;
    uint32* mDescriptors;
};

/*************************/

class nsXPCWrappedJS : public nsXPTCStubBase
{
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD GetInterfaceInfo(nsIInterfaceInfo** info);

    NS_IMETHOD CallMethod(PRUint16 methodIndex,
                          const nsXPTMethodInfo* info,
                          nsXPTCMiniVariant* params);

    static nsXPCWrappedJS* GetNewOrUsedWrapper(XPCContext* xpcc,
                                               JSObject* aJSObj,
                                               REFNSIID aIID);

    JSObject* GetJSObject() const {return mJSObj;}
    nsXPCWrappedJSClass*  GetClass() const {return mClass;}
    REFNSIID GetIID() const {return GetClass()->GetIID();}
    nsXPCWrappedJS* GetRootWrapper() const {return mRoot;}
    void DebugDump(int depth);

    void XPCContextBeingDestroyed();
    nsXPCWrappedJS* Find(REFNSIID aIID);

    virtual ~nsXPCWrappedJS();
private:
    nsXPCWrappedJS();   // not implemented
    nsXPCWrappedJS(JSObject* aJSObj,
                   nsXPCWrappedJSClass* aClass,
                   nsXPCWrappedJS* root);

private:
    JSObject* mJSObj;
    nsXPCWrappedJSClass* mClass;
    nsXPCWrappedJSMethods* mMethods;
    nsXPCWrappedJS* mRoot;
    nsXPCWrappedJS* mNext;
};

class nsXPCWrappedJSMethods : public nsIXPConnectWrappedJSMethods
{
public:
    NS_DECL_ISUPPORTS
    NS_IMETHOD GetJSObject(JSObject** aJSObj);
    NS_IMETHOD GetInterfaceInfo(nsIInterfaceInfo** info);
    NS_IMETHOD GetIID(nsIID** iid); // returns IAllocatator alloc'd copy
    NS_IMETHOD DebugDump(int depth);

    nsXPCWrappedJSMethods(nsXPCWrappedJS* aWrapper);
    virtual ~nsXPCWrappedJSMethods();
    // used in nsXPCWrappedJS::DebugDump
    int GetRefCnt() const {return mRefCnt;}

private:
    nsXPCWrappedJSMethods();  // not implemented

private:
    nsXPCWrappedJS* mWrapper;
};

/***************************************************************************/

// nsXPCWrappedNativeClass maintains an array of these things
class XPCNativeMemberDescriptor
{
private:
    enum {
        // these are all bitwise flags!
        NMD_CONSTANT    = 0x0, // categories...
        NMD_METHOD      = 0x1,
        NMD_ATTRIB_RO   = 0x2,
        NMD_ATTRIB_RW   = 0x3,
        NMD_CAT_MASK    = 0x3  // & mask for the categories above
        // any new bits start at 0x04
    };

public:
    JSObject*       invokeFuncObj;
    jsid            id;  /* hashed name for quick JS property lookup */
    uintN           index; /* in InterfaceInfo for const, method, and get */
    uintN           index2; /* in InterfaceInfo for set */
private:
    uint16          flags;
public:

    JSBool IsConstant() const  {return (flags & NMD_CAT_MASK) == NMD_CONSTANT;}
    JSBool IsMethod() const    {return (flags & NMD_CAT_MASK) == NMD_METHOD;}
    JSBool IsAttribute() const {return (JSBool)((flags & NMD_CAT_MASK) & NMD_ATTRIB_RO);}
    JSBool IsWritableAttribute() const {return (flags & NMD_CAT_MASK) == NMD_ATTRIB_RW;}
    JSBool IsReadOnlyAttribute() const {return (flags & NMD_CAT_MASK) == NMD_ATTRIB_RO;}

    void SetConstant()          {flags=(flags&~NMD_CAT_MASK)|NMD_CONSTANT;}
    void SetMethod()            {flags=(flags&~NMD_CAT_MASK)|NMD_METHOD;}
    void SetReadOnlyAttribute() {flags=(flags&~NMD_CAT_MASK)|NMD_ATTRIB_RO;}
    void SetWritableAttribute() {flags=(flags&~NMD_CAT_MASK)|NMD_ATTRIB_RW;}

    XPCNativeMemberDescriptor();
};

/*************************/

// this interfaces exists just so we can refcount nsXPCWrappedNativeClass
// {C9E36280-954A-11d2-BA5A-00805F8A5DD7}
#define NS_IXPCONNECT_WRAPPED_NATIVE_CLASS_IID \
{ 0xc9e36280, 0x954a, 0x11d2,                   \
  { 0xba, 0x5a, 0x0, 0x80, 0x5f, 0x8a, 0x5d, 0xd7 } }

class nsIXPCWrappedNativeClass : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IXPCONNECT_WRAPPED_NATIVE_CLASS_IID)
    NS_IMETHOD DebugDump(int depth) = 0;
};

/*************************/

class nsXPCWrappedNativeClass : public nsIXPCWrappedNativeClass
{
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_IMETHOD DebugDump(int depth);
public:
    static nsXPCWrappedNativeClass* GetNewOrUsedClass(XPCContext* xpcc,
                                                      REFNSIID aIID);

    REFNSIID GetIID() const {return mIID;}
    const char* GetInterfaceName();
    const char* GetMemberName(const XPCNativeMemberDescriptor* desc) const;
    nsIInterfaceInfo* GetInterfaceInfo() const {return mInfo;}
    XPCContext*  GetXPCContext() const {return mXPCContext;}
    JSContext* GetJSContext() const
        {return mXPCContext ? mXPCContext->GetJSContext() : NULL;}
    nsIXPCScriptable* GetArbitraryScriptable() const
        {return mXPCContext ?
                    mXPCContext->GetXPConnect()->GetArbitraryScriptable() :
                    NULL;}

    static JSBool InitForContext(XPCContext* xpcc);
    static JSBool OneTimeInit();

    JSObject* NewInstanceJSObject(nsXPCWrappedNative* self);
    static nsXPCWrappedNative* GetWrappedNativeOfJSObject(JSContext* cx,
                                                          JSObject* jsobj);

    int GetMemberCount() const {return mMemberCount;}
    const XPCNativeMemberDescriptor* GetMemberDescriptor(uint16 i) const
    {
        NS_PRECONDITION(i < mMemberCount,"bad index");
        return mDescriptors ? &mDescriptors[i] : NULL;
    }

    const XPCNativeMemberDescriptor* LookupMemberByID(jsid id) const;

    JSBool GetConstantAsJSVal(JSContext* cx,
                              nsXPCWrappedNative* wrapper,
                              const XPCNativeMemberDescriptor* desc,
                              jsval* vp);

    enum CallMode {CALL_METHOD, CALL_GETTER, CALL_SETTER};

    JSBool CallWrappedMethod(JSContext* cx,
                             nsXPCWrappedNative* wrapper,
                             const XPCNativeMemberDescriptor* desc,
                             CallMode callMode,
                             uintN argc, jsval *argv, jsval *vp);

    JSBool GetAttributeAsJSVal(JSContext* cx,
                               nsXPCWrappedNative* wrapper,
                               const XPCNativeMemberDescriptor* desc,
                               jsval* vp)
    {
        return CallWrappedMethod(cx, wrapper, desc, CALL_GETTER, 0, NULL, vp);
    }

    JSBool SetAttributeFromJSVal(JSContext* cx,
                                 nsXPCWrappedNative* wrapper,
                                 const XPCNativeMemberDescriptor* desc,
                                 jsval* vp)
    {
        return CallWrappedMethod(cx, wrapper, desc, CALL_SETTER, 1, vp, NULL);
    }

    JSObject* GetInvokeFunObj(const XPCNativeMemberDescriptor* desc);

    JSBool DynamicEnumerate(nsXPCWrappedNative* wrapper,
                            nsIXPCScriptable* ds,
                            nsIXPCScriptable* as,
                            JSContext *cx, JSObject *obj,
                            JSIterateOp enum_op,
                            jsval *statep, jsid *idp);

    JSBool StaticEnumerate(nsXPCWrappedNative* wrapper,
                           JSIterateOp enum_op,
                           jsval *statep, jsid *idp);

    void XPCContextBeingDestroyed();

    virtual ~nsXPCWrappedNativeClass();
private:
    nsXPCWrappedNativeClass();   // not implemented
    nsXPCWrappedNativeClass(XPCContext* xpcc, REFNSIID aIID,
                            nsIInterfaceInfo* aInfo);

    void ThrowBadResultException(JSContext* cx,
                                 const XPCNativeMemberDescriptor* desc,
                                 nsresult result)
        {nsXPConnect::GetJSThrower()->
                ThrowBadResultException(NS_ERROR_XPC_NATIVE_RETURNED_FAILURE,
                                        cx, this, desc, result);}

    void ThrowBadParamException(uintN errNum,
                                JSContext* cx,
                                const XPCNativeMemberDescriptor* desc,
                                uintN paramNum)
        {nsXPConnect::GetJSThrower()->
                ThrowBadParamException(errNum, cx, this, desc, paramNum);}

    void ThrowException(uintN errNum,
                        JSContext* cx,
                        const XPCNativeMemberDescriptor* desc)
        {nsXPConnect::GetJSThrower()->
                ThrowException(errNum, cx, this, desc);}

    JSBool BuildMemberDescriptors();
    void  DestroyMemberDescriptors();

private:
    XPCContext* mXPCContext;
    nsIID mIID;
    char* mName;
    nsIInterfaceInfo* mInfo;
    int mMemberCount;
    XPCNativeMemberDescriptor* mDescriptors;
};

/*************************/

class nsXPCWrappedNative : public nsIXPConnectWrappedNative
{
    // all the interface method declarations...
    NS_DECL_ISUPPORTS

    NS_IMETHOD GetDynamicScriptable(nsIXPCScriptable** p);
    NS_IMETHOD GetArbitraryScriptable(nsIXPCScriptable** p);
    NS_IMETHOD GetJSObject(JSObject** aJSObj);
    NS_IMETHOD GetNative(nsISupports** aObj);
    NS_IMETHOD GetInterfaceInfo(nsIInterfaceInfo** info);
    NS_IMETHOD GetIID(nsIID** iid); // returns IAllocatator alloc'd copy
    NS_IMETHOD DebugDump(int depth);
    NS_IMETHOD SetFinalizeListener(nsIXPConnectFinalizeListener* aListener);
    NS_IMETHOD GetJSObjectPrototype(JSObject** aJSObj);

public:
    static nsXPCWrappedNative* GetNewOrUsedWrapper(XPCContext* xpcc,
                                                   nsISupports* aObj,
                                                   REFNSIID aIID);
    nsISupports* GetNative() const {return mObj;}
    JSObject* GetJSObject() const {return mJSObj;}
    nsXPCWrappedNativeClass* GetClass() const {return mClass;}
    REFNSIID GetIID() const {return GetClass()->GetIID();}

    nsIXPCScriptable* GetDynamicScriptable() const
        {return mRoot->mDynamicScriptable;}

    JSUint32 GetDynamicScriptableFlags() const
        {return mRoot->mDynamicScriptableFlags;}

    nsIXPCScriptable* GetArbitraryScriptable() const
        {return GetClass()->GetArbitraryScriptable();}

    void JSObjectFinalized(JSContext *cx, JSObject *obj);

    void XPCContextBeingDestroyed();

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
    nsIXPCScriptable* mDynamicScriptable;   // only set in root!
    JSUint32 mDynamicScriptableFlags;   // only set in root!
    nsXPCWrappedNative* mRoot;
    nsXPCWrappedNative* mNext;
    nsIXPConnectFinalizeListener* mFinalizeListener;
};

/***************************************************************************/

class nsXPCArbitraryScriptable : public nsIXPCScriptable
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    XPC_DECLARE_IXPCSCRIPTABLE

public:
    nsXPCArbitraryScriptable();
};

/***************************************************************************/
// data convertion

// class here just for static methods
class XPCConvert
{
public:
    static JSBool IsMethodReflectable(const nsXPTMethodInfo& info);

    static JSBool NativeData2JS(JSContext* cx, jsval* d, const void* s,
                                const nsXPTType& type, const nsID* iid,
                                uintN* pErr);

    static JSBool JSData2Native(JSContext* cx, void* d, jsval s,
                                const nsXPTType& type,
                                JSBool useAllocator, const nsID* iid,
                                uintN* pErr);

    static nsIXPCException* JSValToXPCException(JSContext* cx,
                                                jsval s,
                                                const char* ifaceName,
                                                const char* methodName);

    static nsIXPCException* JSErrorToXPCException(JSContext* cx,
                                                  const char* message,
                                                  const JSErrorReport* report);

private:
    XPCConvert(); // not implemented
};

extern JSBool JS_DLL_CALLBACK
XPC_JSArgumentFormatter(JSContext *cx, const char *format,
                        JSBool fromJS, jsval **vpp, va_list *app);

/***************************************************************************/
/*
* nsJSID implements nsIJSID. It is also used by nsJSIID and nsJSCID as a
* member (as a hidden implementaion detail) to which they delegate many calls.
*/

class nsJSID : public nsIJSID
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIJSID

    PRBool initWithName(const nsID& id, const char *nameString);
    PRBool setName(const char* name);
    void   setNameToNoString()
        {NS_ASSERTION(!mName, "name already set"); mName = gNoString;}
    PRBool nameIsSet() const {return nsnull != mName;}
    const nsID* getID() const {return &mID;}

    static nsJSID* NewID(const char* str);

    nsJSID();
    ~nsJSID();
protected:

    void reset();
    const nsID& GetInvalidIID() const;

protected:
    static char gNoString[];
    nsID    mID;
    char*   mNumber;
    char*   mName;
};

// nsJSIID

class nsJSIID : public nsIJSIID
{
public:
    NS_DECL_ISUPPORTS
    // we manually delagate these to nsJSID
    NS_DECL_NSIJSID

    // we implement the rest...
    NS_DECL_NSIJSIID

    static nsJSIID* NewID(const char* str);

    nsJSIID();
    virtual ~nsJSIID();

private:
    void resolveName();

private:
    nsJSID mDetails;
};

// nsJSCID

class nsJSCID : public nsIJSCID
{
public:
    NS_DECL_ISUPPORTS
    // we manually delagate these to nsJSID
    NS_DECL_NSIJSID

    // we implement the rest...
    NS_DECL_NSIJSCID

    static nsJSCID* NewID(const char* str);

    nsJSCID();
    virtual ~nsJSCID();

private:
    void resolveName();

private:
    nsJSID mDetails;
};


JSObject*
xpc_NewIDObject(JSContext *cx, const nsID& aID);

nsID*
xpc_JSObjectToID(JSContext *cx, JSObject* obj);

/***************************************************************************/
// 'Components' objects

class nsXPCInterfaces;
class nsXPCClasses;
class nsXPCClassesByID;
class nsXPCResults;

class nsXPCComponents : public nsIXPCComponents, public nsIXPCScriptable
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCCOMPONENTS
    XPC_DECLARE_IXPCSCRIPTABLE

    nsXPCComponents();
    virtual ~nsXPCComponents();
private:
    nsXPCInterfaces*      mInterfaces;
    nsXPCClasses*         mClasses;
    nsXPCClassesByID*     mClassesByID;
    nsXPCResults*         mResults;
    PRBool                mCreating;
};

/***************************************************************************/

extern JSClass WrappedNative_class;
extern JSClass WrappedNativeWithCall_class;

extern JSBool JS_DLL_CALLBACK
WrappedNative_CallMethod(JSContext *cx, JSObject *obj,
                         uintN argc, jsval *argv, jsval *vp);

extern JSBool xpc_WrappedNativeJSOpsOneTimeInit();

/***************************************************************************/

#define NS_XPC_THREAD_JSCONTEXT_STACK_CID  \
{ 0xff8c4d10, 0x3194, 0x11d3, \
    { 0x98, 0x85, 0x0, 0x60, 0x8, 0x96, 0x24, 0x22 } }

class nsXPCThreadJSContextStackImpl : public nsIJSContextStack
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIJSCONTEXTSTACK

public:
    static nsXPCThreadJSContextStackImpl* GetSingleton();

private:
    // hide ctor and dtor
    nsXPCThreadJSContextStackImpl();
    virtual ~nsXPCThreadJSContextStackImpl();
};

/***************************************************************************/
// a class to put on the stack when we are entering xpconnect from an entry
// point where the JSContext is known. This pushes and pops the given context
// with the nsThreadJSContextStack service as this object goes into and out
// of scope.

class AutoPushJSContext
{
public:
    AutoPushJSContext(JSContext *cx, nsXPConnect* xpc = nsnull);
    ~AutoPushJSContext();
private:
    AutoPushJSContext();    // no implementation

private:
    nsIJSContextStack* mContextStack;
#ifdef DEBUG
    JSContext* mDebugCX;
#endif
};

#define AUTO_PUSH_JSCONTEXT(cx) AutoPushJSContext _AutoPushJSContext(cx)
#define AUTO_PUSH_JSCONTEXT2(cx,xpc) AutoPushJSContext _AutoPushJSContext(cx,xpc)

/***************************************************************************/
class XPCJSStackFrame;

class XPCJSStack
{
public:
    static nsIJSStackFrameLocation* CreateStack(JSContext* cx);

    static nsIJSStackFrameLocation* CreateStackFrameLocation(
                                        JSBool isJSFrame,
                                        const char* aFilename,
                                        const char* aFunctionName,
                                        PRInt32 aLineNumber,
                                        nsIJSStackFrameLocation* aCaller);
friend class XPCJSStackFrame;
private:
    XPCJSStack();
    ~XPCJSStack();

    void AddRef();
    void Release();

    XPCJSStackFrame* mTopFrame;
    int mRefCount;
};

/***************************************************************************/

class nsXPCException : public nsIXPCException
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCEXCEPTION

    static nsXPCException* NewException(const char *aMessage,
                                        nsresult aCode,
                                        nsIJSStackFrameLocation *aLocation,
                                        nsISupports *aData,
                                        PRInt32 aLeadingFramesToTrim);

    static JSBool NameAndFormatForNSResult(nsresult rv,
                                           const char** name,
                                           const char** format);
    static void* IterateNSResults(nsresult* rv,
                                  const char** name,
                                  const char** format,
                                  void** iterp);
    nsXPCException();
    virtual ~nsXPCException();

protected:
    void reset();
private:
    char*                       mMessage;
    nsresult                    mCode;
    char*                       mName;
    nsIJSStackFrameLocation*    mLocation;
    nsISupports*                mData;
    PRBool                      mInitialized;
};

class xpcJSErrorReport : public nsIJSErrorReport
{
    NS_DECL_ISUPPORTS
    NS_DECL_NSIJSERRORREPORT

    static xpcJSErrorReport* NewReport(const char* aMessage,
                                       const JSErrorReport* aReport);

protected:
    xpcJSErrorReport();
    virtual ~xpcJSErrorReport();

private:
    char*       mMessage;
    char*       mFilename;
    PRUint32    mLineno;
    char*       mLinebuf;
    PRUint32    mTokenIndex;
    PRUint32    mFlags;
    PRUint32    mErrorNumber;
};

/***************************************************************************/

// the include of declarations of the maps comes last because they have
// inlines which call methods on classes above.

#include "xpcmaps.h"

#endif /* xpcprivate_h___ */
