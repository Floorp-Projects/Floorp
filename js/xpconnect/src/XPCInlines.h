/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* private inline methods (#include'd by xpcprivate.h). */

#ifndef xpcinlines_h___
#define xpcinlines_h___

#include <algorithm>

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

inline bool
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
    return mWrapper ? mWrapper->GetProto() : nullptr;
}

inline bool
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

inline bool
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

inline bool
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

inline bool
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

inline bool
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

inline bool
XPCNativeInterface::HasAncestor(const nsIID* iid) const
{
    bool found = false;
    mInfo->HasAncestor(iid, &found);
    return found;
}

/***************************************************************************/

inline bool
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

inline bool
XPCNativeSet::FindMember(jsid name, XPCNativeMember** pMember,
                         XPCNativeInterface** pInterface) const
{
    uint16_t index;
    if (!FindMember(name, pMember, &index))
        return false;
    *pInterface = mInterfaces[index];
    return true;
}

inline bool
XPCNativeSet::FindMember(jsid name,
                         XPCNativeMember** pMember,
                         XPCNativeInterface** pInterface,
                         XPCNativeSet* protoSet,
                         bool* pIsLocal) const
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

inline bool
XPCNativeSet::HasInterface(XPCNativeInterface* aInterface) const
{
    XPCNativeInterface* const * pp = mInterfaces;

    for (int i = (int) mInterfaceCount; i > 0; i--, pp++) {
        if (aInterface == *pp)
            return true;
    }
    return false;
}

inline bool
XPCNativeSet::HasInterfaceWithAncestor(XPCNativeInterface* aInterface) const
{
    return HasInterfaceWithAncestor(aInterface->GetIID());
}

inline bool
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

inline bool
XPCNativeSet::MatchesSetUpToInterface(const XPCNativeSet* other,
                                      XPCNativeInterface* iface) const
{
    int count = std::min(int(mInterfaceCount), int(other->mInterfaceCount));

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
    MOZ_ASSERT(!IsMarked(), "bad");

    XPCNativeInterface* const * pp = mInterfaces;

    for (int i = (int) mInterfaceCount; i > 0; i--, pp++)
        MOZ_ASSERT(!(*pp)->IsMarked(), "bad");
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
    MOZ_ASSERT(!(GetInterface() || GetNative() || GetJSObjectPreserveColor()),
               "tearoff not empty in dtor");
}

/***************************************************************************/

inline bool
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
            bool marked = to->IsMarked();
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

inline bool
xpc_ForcePropertyResolve(JSContext* cx, JSObject* obj, jsid id)
{
    JS::RootedValue prop(cx);

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
bool ThrowBadParam(nsresult rv, unsigned paramNum, XPCCallContext& ccx)
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

/***************************************************************************/

#endif /* xpcinlines_h___ */
