/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* private inline methods (#include'd by xpcprivate.h). */

#ifndef xpcinlines_h___
#define xpcinlines_h___

#include "jsfriendapi.h"

/***************************************************************************/

inline void
XPCJSRuntime::AddVariantRoot(XPCTraceableVariant* variant)
{
    variant->AddToRootSet(GetMapLock(), &mVariantRoots);
}

inline void
XPCJSRuntime::AddWrappedJSRoot(nsXPCWrappedJS* wrappedJS)
{
    wrappedJS->AddToRootSet(GetMapLock(), &mWrappedJSRoots);
}

inline void
XPCJSRuntime::AddObjectHolderRoot(XPCJSObjectHolder* holder)
{
    holder->AddToRootSet(GetMapLock(), &mObjectHolderRoots);
}

/***************************************************************************/

inline JSBool
XPCCallContext::IsValid() const
{
    return mState != INIT_FAILED;
}

inline XPCJSRuntime*
XPCCallContext::GetRuntime() const
{
    CHECK_STATE(HAVE_CONTEXT);
    return mXPCContext->GetRuntime();
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
XPCCallContext::GetFlattenedJSObject() const
{
    CHECK_STATE(HAVE_OBJECT);
    return mFlattenedJSObject;
}

inline nsISupports*
XPCCallContext::GetIdentityObject() const
{
    CHECK_STATE(HAVE_OBJECT);
    if (mWrapper)
        return mWrapper->GetIdentityObject();
    return mFlattenedJSObject ?
           static_cast<nsISupports*>(xpc_GetJSPrivate(mFlattenedJSObject)) :
           nullptr;
}

inline XPCWrappedNative*
XPCCallContext::GetWrapper() const
{
    if (mState == INIT_FAILED)
        return nullptr;

    CHECK_STATE(HAVE_OBJECT);
    return mWrapper;
}

inline XPCWrappedNativeProto*
XPCCallContext::GetProto() const
{
    CHECK_STATE(HAVE_OBJECT);
    if (mWrapper)
        return mWrapper->GetProto();
    return mFlattenedJSObject ? GetSlimWrapperProto(mFlattenedJSObject) : nullptr;
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

inline jsid
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

inline unsigned
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

inline void
XPCCallContext::SetRetVal(jsval val)
{
    CHECK_STATE(HAVE_ARGS);
    if (mRetVal)
        *mRetVal = val;
}

inline jsid
XPCCallContext::GetResolveName() const
{
    CHECK_STATE(HAVE_CONTEXT);
    return XPCJSRuntime::Get()->GetResolveName();
}

inline jsid
XPCCallContext::SetResolveName(JS::HandleId name)
{
    CHECK_STATE(HAVE_CONTEXT);
    return XPCJSRuntime::Get()->SetResolveName(name);
}

inline XPCWrappedNative*
XPCCallContext::GetResolvingWrapper() const
{
    CHECK_STATE(HAVE_OBJECT);
    return XPCJSRuntime::Get()->GetResolvingWrapper();
}

inline XPCWrappedNative*
XPCCallContext::SetResolvingWrapper(XPCWrappedNative* w)
{
    CHECK_STATE(HAVE_OBJECT);
    return XPCJSRuntime::Get()->SetResolvingWrapper(w);
}

inline uint16_t
XPCCallContext::GetMethodIndex() const
{
    CHECK_STATE(HAVE_OBJECT);
    return mMethodIndex;
}

inline void
XPCCallContext::SetMethodIndex(uint16_t index)
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
    return NS_SUCCEEDED(mInfo->GetIIDShared(&iid)) ? iid : nullptr;
}

inline const char*
XPCNativeInterface::GetNameString() const
{
    const char* name;
    return NS_SUCCEEDED(mInfo->GetNameShared(&name)) ? name : nullptr;
}

inline XPCNativeMember*
XPCNativeInterface::FindMember(jsid name) const
{
    const XPCNativeMember* member = mMembers;
    for (int i = (int) mMemberCount; i > 0; i--, member++)
        if (member->GetName() == name)
            return const_cast<XPCNativeMember*>(member);
    return nullptr;
}

inline JSBool
XPCNativeInterface::HasAncestor(const nsIID* iid) const
{
    bool found = false;
    mInfo->HasAncestor(iid, &found);
    return found;
}

/***************************************************************************/

inline JSBool
XPCNativeSet::FindMember(jsid name, XPCNativeMember** pMember,
                         uint16_t* pInterfaceIndex) const
{
    XPCNativeInterface* const * iface;
    int count = (int) mInterfaceCount;
    int i;

    // look for interface names first

    for (i = 0, iface = mInterfaces; i < count; i++, iface++) {
        if (name == (*iface)->GetName()) {
            if (pMember)
                *pMember = nullptr;
            if (pInterfaceIndex)
                *pInterfaceIndex = (uint16_t) i;
            return true;
        }
    }

    // look for method names
    for (i = 0, iface = mInterfaces; i < count; i++, iface++) {
        XPCNativeMember* member = (*iface)->FindMember(name);
        if (member) {
            if (pMember)
                *pMember = member;
            if (pInterfaceIndex)
                *pInterfaceIndex = (uint16_t) i;
            return true;
        }
    }
    return false;
}

inline JSBool
XPCNativeSet::FindMember(jsid name, XPCNativeMember** pMember,
                         XPCNativeInterface** pInterface) const
{
    uint16_t index;
    if (!FindMember(name, pMember, &index))
        return false;
    *pInterface = mInterfaces[index];
    return true;
}

inline JSBool
XPCNativeSet::FindMember(jsid name,
                         XPCNativeMember** pMember,
                         XPCNativeInterface** pInterface,
                         XPCNativeSet* protoSet,
                         JSBool* pIsLocal) const
{
    XPCNativeMember* Member;
    XPCNativeInterface* Interface;
    XPCNativeMember* protoMember;

    if (!FindMember(name, &Member, &Interface))
        return false;

    *pMember = Member;
    *pInterface = Interface;

    *pIsLocal =
        !Member ||
        !protoSet ||
        (protoSet != this &&
         !protoSet->MatchesSetUpToInterface(this, Interface) &&
         (!protoSet->FindMember(name, &protoMember, (uint16_t*)nullptr) ||
          protoMember != Member));

    return true;
}

inline XPCNativeInterface*
XPCNativeSet::FindNamedInterface(jsid name) const
{
    XPCNativeInterface* const * pp = mInterfaces;

    for (int i = (int) mInterfaceCount; i > 0; i--, pp++) {
        XPCNativeInterface* iface = *pp;

        if (name == iface->GetName())
            return iface;
    }
    return nullptr;
}

inline XPCNativeInterface*
XPCNativeSet::FindInterfaceWithIID(const nsIID& iid) const
{
    XPCNativeInterface* const * pp = mInterfaces;

    for (int i = (int) mInterfaceCount; i > 0; i--, pp++) {
        XPCNativeInterface* iface = *pp;

        if (iface->GetIID()->Equals(iid))
            return iface;
    }
    return nullptr;
}

inline JSBool
XPCNativeSet::HasInterface(XPCNativeInterface* aInterface) const
{
    XPCNativeInterface* const * pp = mInterfaces;

    for (int i = (int) mInterfaceCount; i > 0; i--, pp++) {
        if (aInterface == *pp)
            return true;
    }
    return false;
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
    for (int i = (int) mInterfaceCount; i > 1; i--, pp++)
        if ((*pp)->HasAncestor(iid))
            return true;

    // This is rare, so check last.
    if (iid == &NS_GET_IID(nsISupports))
        return true;

    return false;
}

inline JSBool
XPCNativeSet::MatchesSetUpToInterface(const XPCNativeSet* other,
                                      XPCNativeInterface* iface) const
{
    int count = js::Min(int(mInterfaceCount), int(other->mInterfaceCount));

    XPCNativeInterface* const * pp1 = mInterfaces;
    XPCNativeInterface* const * pp2 = other->mInterfaces;

    for (int i = (int) count; i > 0; i--, pp1++, pp2++) {
        XPCNativeInterface* cur = (*pp1);
        if (cur != (*pp2))
            return false;
        if (cur == iface)
            return true;
    }
    return false;
}

inline void XPCNativeSet::Mark()
{
    if (IsMarked())
        return;

    XPCNativeInterface* const * pp = mInterfaces;

    for (int i = (int) mInterfaceCount; i > 0; i--, pp++)
        (*pp)->Mark();

    MarkSelfOnly();
}

#ifdef DEBUG
inline void XPCNativeSet::ASSERT_NotMarked()
{
    NS_ASSERTION(!IsMarked(), "bad");

    XPCNativeInterface* const * pp = mInterfaces;

    for (int i = (int) mInterfaceCount; i > 0; i--, pp++)
        NS_ASSERTION(!(*pp)->IsMarked(), "bad");
}
#endif

/***************************************************************************/

inline
JSObject* XPCWrappedNativeTearOff::GetJSObjectPreserveColor() const
{
    return reinterpret_cast<JSObject *>(reinterpret_cast<uintptr_t>(mJSObject) & ~1);
}

inline
JSObject* XPCWrappedNativeTearOff::GetJSObject()
{
    JSObject *obj = GetJSObjectPreserveColor();
    xpc_UnmarkGrayObject(obj);
    return obj;
}

inline
void XPCWrappedNativeTearOff::SetJSObject(JSObject*  JSObj)
{
    MOZ_ASSERT(!IsMarked());
    mJSObject = JSObj;
}

inline
XPCWrappedNativeTearOff::~XPCWrappedNativeTearOff()
{
    NS_ASSERTION(!(GetInterface()||GetNative()||GetJSObjectPreserveColor()),
                 "tearoff not empty in dtor");
}

/***************************************************************************/

inline JSBool
XPCWrappedNative::HasInterfaceNoQI(const nsIID& iid)
{
    return nullptr != GetSet()->FindInterfaceWithIID(iid);
}

inline void
XPCWrappedNative::SweepTearOffs()
{
    XPCWrappedNativeTearOffChunk* chunk;
    for (chunk = &mFirstChunk; chunk; chunk = chunk->mNextChunk) {
        XPCWrappedNativeTearOff* to = chunk->mTearOffs;
        for (int i = XPC_WRAPPED_NATIVE_TEAROFFS_PER_CHUNK; i > 0; i--, to++) {
            JSBool marked = to->IsMarked();
            to->Unmark();
            if (marked)
                continue;

            // If this tearoff does not have a live dedicated JSObject,
            // then let's recycle it.
            if (!to->GetJSObjectPreserveColor()) {
                nsISupports* obj = to->GetNative();
                if (obj) {
                    obj->Release();
                    to->SetNative(nullptr);
                }
                to->SetInterface(nullptr);
            }
        }
    }
}

/***************************************************************************/

inline JSBool
xpc_ForcePropertyResolve(JSContext* cx, JSObject* obj, jsid id)
{
    jsval prop;

    if (!JS_LookupPropertyById(cx, obj, id, &prop))
        return false;
    return true;
}

inline jsid
GetRTIdByIndex(JSContext *cx, unsigned index)
{
  XPCJSRuntime *rt = nsXPConnect::XPConnect()->GetRuntime();
  return rt->GetStringID(index);
}

inline
JSBool ThrowBadParam(nsresult rv, unsigned paramNum, XPCCallContext& ccx)
{
    XPCThrower::ThrowBadParam(rv, paramNum, ccx);
    return false;
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
    if (mTearOff)
        mFlattenedJSObject = mTearOff->GetJSObject();
    else
        mFlattenedJSObject = mWrapper->GetFlatJSObject();
}
inline void
XPCLazyCallContext::SetWrapper(JSObject* flattenedJSObject)
{
    NS_ASSERTION(IS_SLIM_WRAPPER_OBJECT(flattenedJSObject),
                 "What kind of object is this?");
    mWrapper = nullptr;
    mTearOff = nullptr;
    mFlattenedJSObject = flattenedJSObject;
}

/***************************************************************************/

#endif /* xpcinlines_h___ */
