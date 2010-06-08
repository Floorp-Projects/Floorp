/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Bandhauer <jband@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* private inline methods (#include'd by xpcprivate.h). */

#ifndef xpcinlines_h___
#define xpcinlines_h___

/***************************************************************************/
inline void
XPCJSRuntime::AddVariantRoot(XPCTraceableVariant* variant)
{
    variant->AddToRootSet(GetJSRuntime(), &mVariantRoots);
}

inline void
XPCJSRuntime::AddWrappedJSRoot(nsXPCWrappedJS* wrappedJS)
{
    wrappedJS->AddToRootSet(GetJSRuntime(), &mWrappedJSRoots);
}

inline void
XPCJSRuntime::AddObjectHolderRoot(XPCJSObjectHolder* holder)
{
    holder->AddToRootSet(GetJSRuntime(), &mObjectHolderRoots);
}

/***************************************************************************/

inline JSBool
XPCCallContext::IsValid() const
{
    return mState != INIT_FAILED;
}

inline nsXPConnect*
XPCCallContext::GetXPConnect() const
{
    CHECK_STATE(HAVE_CONTEXT);
    return mXPC;
}

inline XPCJSRuntime*
XPCCallContext::GetRuntime() const
{
    CHECK_STATE(HAVE_CONTEXT);
    return mXPCContext->GetRuntime();
}

inline XPCPerThreadData*
XPCCallContext::GetThreadData() const
{
    CHECK_STATE(HAVE_CONTEXT);
    return mThreadData;
}

inline XPCContext*
XPCCallContext::GetXPCContext() const
{
    CHECK_STATE(HAVE_CONTEXT);
    return mXPCContext;
}

inline JSContext*
XPCCallContext::GetJSContext() const
{
    CHECK_STATE(HAVE_CONTEXT);
    return mJSContext;
}

inline JSContext*
XPCCallContext::GetSafeJSContext() const
{
    CHECK_STATE(HAVE_CONTEXT);
    JSContext* cx;
    if(NS_SUCCEEDED(mThreadData->GetJSContextStack()->GetSafeJSContext(&cx)))
        return cx;
    return nsnull;
}

inline JSBool
XPCCallContext::GetContextPopRequired() const
{
    CHECK_STATE(HAVE_CONTEXT);
    return mContextPopRequired;
}

inline XPCContext::LangType
XPCCallContext::GetCallerLanguage() const
{
    CHECK_STATE(HAVE_CONTEXT);
    return mCallerLanguage;
}

inline XPCContext::LangType
XPCCallContext::GetPrevCallerLanguage() const
{
    CHECK_STATE(HAVE_CONTEXT);
    return mPrevCallerLanguage;
}

inline XPCCallContext*
XPCCallContext::GetPrevCallContext() const
{
    CHECK_STATE(HAVE_CONTEXT);
    return mPrevCallContext;
}

inline JSObject*
XPCCallContext::GetOperandJSObject() const
{
    CHECK_STATE(HAVE_OBJECT);
    return mOperandJSObject;
}

inline JSObject*
XPCCallContext::GetCurrentJSObject() const
{
    CHECK_STATE(HAVE_OBJECT);
    return mCurrentJSObject;
}

inline JSObject*
XPCCallContext::GetFlattenedJSObject() const
{
    CHECK_STATE(HAVE_OBJECT);
    return mFlattenedJSObject;
}

inline nsISupports*
XPCCallContext::GetIdentityObject() const
{
    CHECK_STATE(HAVE_OBJECT);
    if(mWrapper)
        return mWrapper->GetIdentityObject();
    return mCurrentJSObject ?
           static_cast<nsISupports*>(xpc_GetJSPrivate(mCurrentJSObject)) :
           nsnull;
}

inline XPCWrappedNative*
XPCCallContext::GetWrapper() const
{
    if(mState == INIT_FAILED)
        return nsnull;

    CHECK_STATE(HAVE_OBJECT);
    return mWrapper;
}

inline XPCWrappedNativeProto*
XPCCallContext::GetProto() const
{
    CHECK_STATE(HAVE_OBJECT);
    if(mWrapper)
        return mWrapper->GetProto();
    return mCurrentJSObject ? GetSlimWrapperProto(mCurrentJSObject) : nsnull;
}

inline JSBool
XPCCallContext::CanGetTearOff() const
{
    return mState >= HAVE_OBJECT;
}

inline XPCWrappedNativeTearOff*
XPCCallContext::GetTearOff() const
{
    CHECK_STATE(HAVE_OBJECT);
    return mTearOff;
}

inline XPCNativeScriptableInfo*
XPCCallContext::GetScriptableInfo() const
{
    CHECK_STATE(HAVE_OBJECT);
    return mScriptableInfo;
}

inline JSBool
XPCCallContext::CanGetSet() const
{
    return mState >= HAVE_NAME;
}

inline XPCNativeSet*
XPCCallContext::GetSet() const
{
    CHECK_STATE(HAVE_NAME);
    return mSet;
}

inline JSBool
XPCCallContext::CanGetInterface() const
{
    return mState >= HAVE_NAME;
}

inline XPCNativeInterface*
XPCCallContext::GetInterface() const
{
    CHECK_STATE(HAVE_NAME);
    return mInterface;
}

inline XPCNativeMember*
XPCCallContext::GetMember() const
{
    CHECK_STATE(HAVE_NAME);
    return mMember;
}

inline JSBool
XPCCallContext::HasInterfaceAndMember() const
{
    return mState >= HAVE_NAME && mInterface && (mMember
#ifdef XPC_IDISPATCH_SUPPORT
        || mIDispatchMember
#endif
        );
}

inline jsval
XPCCallContext::GetName() const
{
    CHECK_STATE(HAVE_NAME);
    return mName;
}

inline JSBool
XPCCallContext::GetStaticMemberIsLocal() const
{
    CHECK_STATE(HAVE_NAME);
    return mStaticMemberIsLocal;
}

inline uintN
XPCCallContext::GetArgc() const
{
    CHECK_STATE(READY_TO_CALL);
    return mArgc;
}

inline jsval*
XPCCallContext::GetArgv() const
{
    CHECK_STATE(READY_TO_CALL);
    return mArgv;
}

inline jsval*
XPCCallContext::GetRetVal() const
{
    CHECK_STATE(READY_TO_CALL);
    return mRetVal;
}

inline JSBool
XPCCallContext::GetReturnValueWasSet() const
{
    CHECK_STATE(READY_TO_CALL);
    return mReturnValueWasSet;
}

inline void
XPCCallContext::SetRetVal(jsval val)
{
    CHECK_STATE(HAVE_ARGS);
    if(mRetVal)
        *mRetVal = val;
}

inline jsval
XPCCallContext::GetResolveName() const
{
    CHECK_STATE(HAVE_CONTEXT);
    return mThreadData->GetResolveName();
}

inline jsval
XPCCallContext::SetResolveName(jsval name)
{
    CHECK_STATE(HAVE_CONTEXT);
    return mThreadData->SetResolveName(name);
}

inline XPCWrappedNative*
XPCCallContext::GetResolvingWrapper() const
{
    CHECK_STATE(HAVE_OBJECT);
    return mThreadData->GetResolvingWrapper();
}

inline XPCWrappedNative*
XPCCallContext::SetResolvingWrapper(XPCWrappedNative* w)
{
    CHECK_STATE(HAVE_OBJECT);
    return mThreadData->SetResolvingWrapper(w);
}

inline JSObject*
XPCCallContext::GetCallee() const
{
    NS_ASSERTION(mCallerLanguage == NATIVE_CALLER,
                 "GetCallee() doesn't make sense");
    return mCallee;
}

inline void
XPCCallContext::SetCallee(JSObject* callee)
{
    NS_ASSERTION(mCallerLanguage == NATIVE_CALLER,
                 "SetCallee() doesn't make sense");
    mCallee = callee;
}

inline PRUint16
XPCCallContext::GetMethodIndex() const
{
    CHECK_STATE(HAVE_OBJECT);
    return mMethodIndex;
}

inline void
XPCCallContext::SetMethodIndex(PRUint16 index)
{
    CHECK_STATE(HAVE_OBJECT);
    mMethodIndex = index;
}

inline JSBool   
XPCCallContext::GetDestroyJSContextInDestructor() const
{
    CHECK_STATE(HAVE_CONTEXT);
    return mDestroyJSContextInDestructor;
}

inline void
XPCCallContext::SetDestroyJSContextInDestructor(JSBool b)
{
    CHECK_STATE(HAVE_CONTEXT);
    mDestroyJSContextInDestructor = b;
}

/***************************************************************************/

inline const nsIID*
XPCNativeInterface::GetIID() const
{
    const nsIID* iid;
    return NS_SUCCEEDED(mInfo->GetIIDShared(&iid)) ? iid : nsnull;
}

inline const char*
XPCNativeInterface::GetNameString() const
{
    const char* name;
    return NS_SUCCEEDED(mInfo->GetNameShared(&name)) ? name : nsnull;
}

inline XPCNativeMember*
XPCNativeInterface::FindMember(jsval name) const
{
    const XPCNativeMember* member = mMembers;
    for(int i = (int) mMemberCount; i > 0; i--, member++)
        if(member->GetName() == name)
            return const_cast<XPCNativeMember*>(member);
    return nsnull;
}

inline JSBool
XPCNativeInterface::HasAncestor(const nsIID* iid) const
{
    PRBool found = PR_FALSE;
    mInfo->HasAncestor(iid, &found);
    return found;
}

inline void
XPCNativeInterface::DealWithDyingGCThings(JSContext* cx, XPCJSRuntime* rt)
{
    XPCNativeMember* member = mMembers;
    for(int i = (int) mMemberCount; i > 0; i--, member++)
        member->DealWithDyingGCThings(cx, rt);
}

/***************************************************************************/

inline JSBool
XPCNativeSet::FindMember(jsval name, XPCNativeMember** pMember,
                         PRUint16* pInterfaceIndex) const
{
    XPCNativeInterface* const * iface;
    int count = (int) mInterfaceCount;
    int i;

    // look for interface names first

    for(i = 0, iface = mInterfaces; i < count; i++, iface++)
    {
        if(name == (*iface)->GetName())
        {
            if(pMember)
                *pMember = nsnull;
            if(pInterfaceIndex)
                *pInterfaceIndex = (PRUint16) i;
            return JS_TRUE;
        }
    }

    // look for method names
    for(i = 0, iface = mInterfaces; i < count; i++, iface++)
    {
        XPCNativeMember* member = (*iface)->FindMember(name);
        if(member)
        {
            if(pMember)
                *pMember = member;
            if(pInterfaceIndex)
                *pInterfaceIndex = (PRUint16) i;
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}

inline JSBool
XPCNativeSet::FindMember(jsval name, XPCNativeMember** pMember,
                         XPCNativeInterface** pInterface) const
{
    PRUint16 index;
    if(!FindMember(name, pMember, &index))
        return JS_FALSE;
    *pInterface = mInterfaces[index];
    return JS_TRUE;
}

inline JSBool
XPCNativeSet::FindMember(jsval name,
                         XPCNativeMember** pMember,
                         XPCNativeInterface** pInterface,
                         XPCNativeSet* protoSet,
                         JSBool* pIsLocal) const
{
    XPCNativeMember* Member;
    XPCNativeInterface* Interface;
    XPCNativeMember* protoMember;

    if(!FindMember(name, &Member, &Interface))
        return JS_FALSE;

    *pMember = Member;
    *pInterface = Interface;

    *pIsLocal =
        !Member ||
        !protoSet ||
        (protoSet != this &&
         !protoSet->MatchesSetUpToInterface(this, Interface) &&
          (!protoSet->FindMember(name, &protoMember, (PRUint16*)nsnull) ||
           protoMember != Member));

    return JS_TRUE;
}

inline XPCNativeInterface*
XPCNativeSet::FindNamedInterface(jsval name) const
{
    XPCNativeInterface* const * pp = mInterfaces;

    for(int i = (int) mInterfaceCount; i > 0; i--, pp++)
    {
        XPCNativeInterface* iface = *pp;

        if(name == iface->GetName())
            return iface;
    }
    return nsnull;
}

inline XPCNativeInterface*
XPCNativeSet::FindInterfaceWithIID(const nsIID& iid) const
{
    XPCNativeInterface* const * pp = mInterfaces;

    for(int i = (int) mInterfaceCount; i > 0; i--, pp++)
    {
        XPCNativeInterface* iface = *pp;

        if(iface->GetIID()->Equals(iid))
            return iface;
    }
    return nsnull;
}

inline JSBool
XPCNativeSet::HasInterface(XPCNativeInterface* aInterface) const
{
    XPCNativeInterface* const * pp = mInterfaces;

    for(int i = (int) mInterfaceCount; i > 0; i--, pp++)
    {
        if(aInterface == *pp)
            return JS_TRUE;
    }
    return JS_FALSE;
}

inline JSBool
XPCNativeSet::HasInterfaceWithAncestor(XPCNativeInterface* aInterface) const
{
    return HasInterfaceWithAncestor(aInterface->GetIID());
}

inline JSBool
XPCNativeSet::HasInterfaceWithAncestor(const nsIID* iid) const
{
    // We can safely skip the first interface which is *always* nsISupports.
    XPCNativeInterface* const * pp = mInterfaces+1;
    for(int i = (int) mInterfaceCount; i > 1; i--, pp++)
        if((*pp)->HasAncestor(iid))
            return JS_TRUE;

    // This is rare, so check last.
    if(iid == &NS_GET_IID(nsISupports))
        return PR_TRUE;

    return JS_FALSE;
}

inline JSBool
XPCNativeSet::MatchesSetUpToInterface(const XPCNativeSet* other,
                                      XPCNativeInterface* iface) const
{
    int count = JS_MIN((int)mInterfaceCount, (int)other->mInterfaceCount);

    XPCNativeInterface* const * pp1 = mInterfaces;
    XPCNativeInterface* const * pp2 = other->mInterfaces;

    for(int i = (int) count; i > 0; i--, pp1++, pp2++)
    {
        XPCNativeInterface* cur = (*pp1);
        if(cur != (*pp2))
            return JS_FALSE;
        if(cur == iface)
            return JS_TRUE;
    }
    return JS_FALSE;
}

inline void XPCNativeSet::Mark()
{
    if(IsMarked())
        return;

    XPCNativeInterface* const * pp = mInterfaces;

    for(int i = (int) mInterfaceCount; i > 0; i--, pp++)
        (*pp)->Mark();

    MarkSelfOnly();
}

#ifdef DEBUG
inline void XPCNativeSet::ASSERT_NotMarked()
{
    NS_ASSERTION(!IsMarked(), "bad");

    XPCNativeInterface* const * pp = mInterfaces;

    for(int i = (int) mInterfaceCount; i > 0; i--, pp++)
        NS_ASSERTION(!(*pp)->IsMarked(), "bad");
}
#endif

/***************************************************************************/

inline
JSObject* XPCWrappedNativeTearOff::GetJSObject() const
{
#ifdef XPC_IDISPATCH_SUPPORT
    if(IsIDispatch())
    {
        XPCDispInterface * iface = GetIDispatchInfo();
        return iface ? iface->GetJSObject() : nsnull;
    }
#endif
    return mJSObject;
}

inline
void XPCWrappedNativeTearOff::SetJSObject(JSObject*  JSObj)
{
#ifdef XPC_IDISPATCH_SUPPORT
    if(IsIDispatch())
    {
        XPCDispInterface* iface = GetIDispatchInfo();
        if(iface)
            iface->SetJSObject(JSObj);
    }
    else
#endif
        mJSObject = JSObj;
}

#ifdef XPC_IDISPATCH_SUPPORT
inline void
XPCWrappedNativeTearOff::SetIDispatch(JSContext* cx)
{
    mJSObject = (JSObject*)(((jsword)
        ::XPCDispInterface::NewInstance(cx,
                                          mNative)) | 2);
}

inline XPCDispInterface* 
XPCWrappedNativeTearOff::GetIDispatchInfo() const
{
    NS_ASSERTION((jsword)mJSObject & 2, "XPCWrappedNativeTearOff::GetIDispatchInfo "
                                "called on a non IDispatch interface");
    return reinterpret_cast<XPCDispInterface*>
                           ((((jsword)mJSObject) & ~JSOBJECT_MASK));
}

inline JSBool
XPCWrappedNativeTearOff::IsIDispatch() const
{
    return (JSBool)(((jsword)mJSObject) & IDISPATCH_BIT);
}

#endif

inline
XPCWrappedNativeTearOff::~XPCWrappedNativeTearOff()
{
    NS_ASSERTION(!(GetInterface()||GetNative()||GetJSObject()), "tearoff not empty in dtor");
#ifdef XPC_IDISPATCH_SUPPORT
    if(IsIDispatch())
        delete GetIDispatchInfo();
#endif
}

/***************************************************************************/

inline JSBool
XPCWrappedNative::HasInterfaceNoQI(const nsIID& iid)
{
    return nsnull != GetSet()->FindInterfaceWithIID(iid);
}

inline void
XPCWrappedNative::SweepTearOffs()
{
    XPCWrappedNativeTearOffChunk* chunk;
    for(chunk = &mFirstChunk; chunk; chunk = chunk->mNextChunk)
    {
        XPCWrappedNativeTearOff* to = chunk->mTearOffs;
        for(int i = XPC_WRAPPED_NATIVE_TEAROFFS_PER_CHUNK; i > 0; i--, to++)
        {
            JSBool marked = to->IsMarked();
            to->Unmark();
            if(marked)
                continue;

            // If this tearoff does not have a live dedicated JSObject,
            // then let's recycle it.
            if(!to->GetJSObject())
            {
                nsISupports* obj = to->GetNative();
                if(obj)
                {
                    obj->Release();
                    to->SetNative(nsnull);
                }
                to->SetInterface(nsnull);
            }
        }
    }
}

/***************************************************************************/

inline JSBool
xpc_ForcePropertyResolve(JSContext* cx, JSObject* obj, jsval idval)
{
    jsval prop;
    jsid id;

    if(!JS_ValueToId(cx, idval, &id) ||
       !JS_LookupPropertyById(cx, obj, id, &prop))
        return JS_FALSE;
    return JS_TRUE;
}

inline JSObject*
xpc_NewSystemInheritingJSObject(JSContext *cx, JSClass *clasp, JSObject *proto,
                                JSObject *parent)
{
    JSObject *obj;
    if (clasp->flags & JSCLASS_IS_GLOBAL) {
        obj = JS_NewGlobalObject(cx, clasp);
        if (obj && proto)
            JS_SetPrototype(cx, obj, proto);
    } else {
        obj = JS_NewObject(cx, clasp, proto, parent);
    }
    if (obj && JS_IsSystemObject(cx, parent) && !JS_MakeSystemObject(cx, obj))
        obj = NULL;
    return obj;
}

inline JSBool
xpc_SameScope(XPCWrappedNativeScope *objectscope, XPCWrappedNativeScope *xpcscope,
              JSBool *sameOrigin)
{
    if (objectscope == xpcscope)
    {
        *sameOrigin = JS_TRUE;
        return JS_TRUE;
    }

    nsIPrincipal *objectprincipal = objectscope->GetPrincipal();
    nsIPrincipal *xpcprincipal = xpcscope->GetPrincipal();
    if(!objectprincipal || !xpcprincipal ||
       NS_FAILED(objectprincipal->Equals(xpcprincipal, sameOrigin)))
    {
        *sameOrigin = JS_FALSE;
    }

    return JS_FALSE;
}

inline jsid
GetRTIdByIndex(JSContext *cx, uintN index)
{
  XPCJSRuntime *rt = nsXPConnect::FastGetXPConnect()->GetRuntime();
  return rt->GetStringID(index);
}

inline jsval
GetRTStringByIndex(JSContext *cx, uintN index)
{
  return ID_TO_VALUE(GetRTIdByIndex(cx, index));
}

inline
JSBool ThrowBadParam(nsresult rv, uintN paramNum, XPCCallContext& ccx)
{
    XPCThrower::ThrowBadParam(rv, paramNum, ccx);
    return JS_FALSE;
}

inline 
void ThrowBadResult(nsresult result, XPCCallContext& ccx)
{
    XPCThrower::ThrowBadResult(NS_ERROR_XPC_NATIVE_RETURNED_FAILURE,
                               result, ccx);
}

inline void
XPCLazyCallContext::SetWrapper(XPCWrappedNative* wrapper,
                               XPCWrappedNativeTearOff* tearoff)
{
    mWrapper = wrapper;
    mTearOff = tearoff;
    if(mTearOff)
        mCurrentJSObject = mTearOff->GetJSObject();
    else
        mWrapper->GetJSObject(&mCurrentJSObject);
}
inline void
XPCLazyCallContext::SetWrapper(JSObject* currentJSObject)
{
    NS_ASSERTION(IS_SLIM_WRAPPER_OBJECT(currentJSObject),
                 "What kind of object is this?");
    mWrapper = nsnull;
    mTearOff = nsnull;
    mCurrentJSObject = currentJSObject;
}

/***************************************************************************/

#endif /* xpcinlines_h___ */
