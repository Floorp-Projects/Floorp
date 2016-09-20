/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* private inline methods (#include'd by xpcprivate.h). */

#ifndef xpcinlines_h___
#define xpcinlines_h___

#include <algorithm>

/***************************************************************************/

inline void
XPCJSContext::AddVariantRoot(XPCTraceableVariant* variant)
{
    variant->AddToRootSet(&mVariantRoots);
}

inline void
XPCJSContext::AddWrappedJSRoot(nsXPCWrappedJS* wrappedJS)
{
    wrappedJS->AddToRootSet(&mWrappedJSRoots);
}

inline void
XPCJSContext::AddObjectHolderRoot(XPCJSObjectHolder* holder)
{
    holder->AddToRootSet(&mObjectHolderRoots);
}

/***************************************************************************/

inline bool
XPCCallContext::IsValid() const
{
    return mState != INIT_FAILED;
}

inline XPCJSContext*
XPCCallContext::GetContext() const
{
    CHECK_STATE(HAVE_CONTEXT);
    return mXPCJSContext;
}

inline JSContext*
XPCCallContext::GetJSContext() const
{
    CHECK_STATE(HAVE_CONTEXT);
    return mJSContext;
}

inline XPCCallContext*
XPCCallContext::GetPrevCallContext() const
{
    CHECK_STATE(HAVE_CONTEXT);
    return mPrevCallContext;
}

inline nsISupports*
XPCCallContext::GetIdentityObject() const
{
    CHECK_STATE(HAVE_OBJECT);
    if (mWrapper)
        return mWrapper->GetIdentityObject();
    return nullptr;
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

inline JS::Value*
XPCCallContext::GetArgv() const
{
    CHECK_STATE(READY_TO_CALL);
    return mArgv;
}

inline JS::Value*
XPCCallContext::GetRetVal() const
{
    CHECK_STATE(READY_TO_CALL);
    return mRetVal;
}

inline void
XPCCallContext::SetRetVal(JS::Value val)
{
    CHECK_STATE(HAVE_ARGS);
    if (mRetVal)
        *mRetVal = val;
}

inline jsid
XPCCallContext::GetResolveName() const
{
    CHECK_STATE(HAVE_CONTEXT);
    return XPCJSContext::Get()->GetResolveName();
}

inline jsid
XPCCallContext::SetResolveName(JS::HandleId name)
{
    CHECK_STATE(HAVE_CONTEXT);
    return XPCJSContext::Get()->SetResolveName(name);
}

inline XPCWrappedNative*
XPCCallContext::GetResolvingWrapper() const
{
    CHECK_STATE(HAVE_OBJECT);
    return XPCJSContext::Get()->GetResolvingWrapper();
}

inline XPCWrappedNative*
XPCCallContext::SetResolvingWrapper(XPCWrappedNative* w)
{
    CHECK_STATE(HAVE_OBJECT);
    return XPCJSContext::Get()->SetResolvingWrapper(w);
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
inline XPCNativeInterface*
XPCNativeMember::GetInterface() const
{
    XPCNativeMember* arrayStart =
        const_cast<XPCNativeMember*>(this - mIndexInInterface);
    size_t arrayStartOffset = XPCNativeInterface::OffsetOfMembers();
    char* xpcNativeInterfaceStart =
        reinterpret_cast<char*>(arrayStart) - arrayStartOffset;
    return reinterpret_cast<XPCNativeInterface*>(xpcNativeInterfaceStart);
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

/* static */
inline size_t
XPCNativeInterface::OffsetOfMembers()
{
    return offsetof(XPCNativeInterface, mMembers);
}

/***************************************************************************/

inline XPCNativeSetKey::XPCNativeSetKey(XPCNativeSet* baseSet,
                                        XPCNativeInterface* addition)
    : mBaseSet(baseSet)
    , mAddition(addition)
{
    MOZ_ASSERT(mBaseSet);
    MOZ_ASSERT(mAddition);
    MOZ_ASSERT(!mBaseSet->HasInterface(mAddition));
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
                         RefPtr<XPCNativeInterface>* pInterface) const
{
    uint16_t index;
    if (!FindMember(name, pMember, &index))
        return false;
    *pInterface = mInterfaces[index];
    return true;
}

inline bool
XPCNativeSet::FindMember(JS::HandleId name,
                         XPCNativeMember** pMember,
                         RefPtr<XPCNativeInterface>* pInterface,
                         XPCNativeSet* protoSet,
                         bool* pIsLocal) const
{
    XPCNativeMember* Member;
    RefPtr<XPCNativeInterface> Interface;
    XPCNativeMember* protoMember;

    if (!FindMember(name, &Member, &Interface))
        return false;

    *pMember = Member;

    *pIsLocal =
        !Member ||
        !protoSet ||
        (protoSet != this &&
         !protoSet->MatchesSetUpToInterface(this, Interface) &&
         (!protoSet->FindMember(name, &protoMember, (uint16_t*)nullptr) ||
          protoMember != Member));

    *pInterface = Interface.forget();

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

/***************************************************************************/

inline
JSObject* XPCWrappedNativeTearOff::GetJSObjectPreserveColor() const
{
    return mJSObject.getPtr();
}

inline
JSObject* XPCWrappedNativeTearOff::GetJSObject()
{
    JSObject* obj = GetJSObjectPreserveColor();
    if (obj) {
      JS::ExposeObjectToActiveJS(obj);
    }
    return obj;
}

inline
void XPCWrappedNativeTearOff::SetJSObject(JSObject*  JSObj)
{
    MOZ_ASSERT(!IsMarked());
    mJSObject = JSObj;
}

inline
void XPCWrappedNativeTearOff::JSObjectMoved(JSObject* obj, const JSObject* old)
{
    MOZ_ASSERT(!IsMarked());
    MOZ_ASSERT(mJSObject == old);
    mJSObject = obj;
}

inline
XPCWrappedNativeTearOff::~XPCWrappedNativeTearOff()
{
    MOZ_COUNT_DTOR(XPCWrappedNativeTearOff);
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
    for (XPCWrappedNativeTearOff* to = &mFirstTearOff; to; to = to->GetNextTearOff()) {
        bool marked = to->IsMarked();
        to->Unmark();
        if (marked)
            continue;

        // If this tearoff does not have a live dedicated JSObject,
        // then let's recycle it.
        if (!to->GetJSObjectPreserveColor()) {
            to->SetNative(nullptr);
            to->SetInterface(nullptr);
        }
    }
}

/***************************************************************************/

inline bool
xpc_ForcePropertyResolve(JSContext* cx, JS::HandleObject obj, jsid idArg)
{
    JS::RootedId id(cx, idArg);
    bool dummy;
    return JS_HasPropertyById(cx, obj, id, &dummy);
}

inline jsid
GetJSIDByIndex(JSContext* cx, unsigned index)
{
  XPCJSContext* xpcx = nsXPConnect::XPConnect()->GetContext();
  return xpcx->GetStringID(index);
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
