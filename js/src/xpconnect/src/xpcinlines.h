/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   John Bandhauer <jband@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/* private inline methods (#include'd by xpcprivate.h). */

#ifndef xpcinlines_h___
#define xpcinlines_h___

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
    return mWrapper->GetIdentityObject();
}

inline XPCWrappedNative*
XPCCallContext::GetWrapper() const
{
    if(mState == INIT_FAILED)
        return nsnull;

    CHECK_STATE(HAVE_OBJECT);
    return mWrapper;
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
    return mState >= HAVE_NAME && mInterface && mMember;
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
XPCCallContext::GetExceptionWasThrown() const
{
    CHECK_STATE(READY_TO_CALL);
    return mExceptionWasThrown;
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
            return NS_CONST_CAST(XPCNativeMember*, member);
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
    const nsIID* iid = aInterface->GetIID();

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

inline JSBool
XPCWrappedNative::HasInterfaceNoQI(XPCNativeInterface* aInterface)
{
    return GetSet()->HasInterface(aInterface);
}

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
    JSProperty* prop;
    JSObject* obj2;
    jsid id;    

    if(!JS_ValueToId(cx, idval, &id) ||
       !OBJ_LOOKUP_PROPERTY(cx, obj, id, &obj2, &prop))
        return JS_FALSE;
    if(prop)
        OBJ_DROP_PROPERTY(cx, obj2, prop);
    return JS_TRUE;
}

/***************************************************************************/

#endif /* xpcinlines_h___ */
