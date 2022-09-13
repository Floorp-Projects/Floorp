/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* private inline methods (#include'd by xpcprivate.h). */

#ifndef xpcinlines_h___
#define xpcinlines_h___

#include <algorithm>

#include "js/PropertyAndElement.h"  // JS_HasProperty, JS_HasPropertyById

/***************************************************************************/

inline bool XPCCallContext::IsValid() const { return mState != INIT_FAILED; }

inline XPCJSContext* XPCCallContext::GetContext() const {
  CHECK_STATE(HAVE_CONTEXT);
  return mXPCJSContext;
}

inline JSContext* XPCCallContext::GetJSContext() const {
  CHECK_STATE(HAVE_CONTEXT);
  return mJSContext;
}

inline XPCCallContext* XPCCallContext::GetPrevCallContext() const {
  CHECK_STATE(HAVE_CONTEXT);
  return mPrevCallContext;
}

inline XPCWrappedNative* XPCCallContext::GetWrapper() const {
  if (mState == INIT_FAILED) {
    return nullptr;
  }

  CHECK_STATE(HAVE_OBJECT);
  return mWrapper;
}

inline bool XPCCallContext::CanGetTearOff() const {
  return mState >= HAVE_OBJECT;
}

inline XPCWrappedNativeTearOff* XPCCallContext::GetTearOff() const {
  CHECK_STATE(HAVE_OBJECT);
  return mTearOff;
}

inline nsIXPCScriptable* XPCCallContext::GetScriptable() const {
  CHECK_STATE(HAVE_OBJECT);
  return mScriptable;
}

inline XPCNativeSet* XPCCallContext::GetSet() const {
  CHECK_STATE(HAVE_NAME);
  return mSet;
}

inline XPCNativeInterface* XPCCallContext::GetInterface() const {
  CHECK_STATE(HAVE_NAME);
  return mInterface;
}

inline XPCNativeMember* XPCCallContext::GetMember() const {
  CHECK_STATE(HAVE_NAME);
  return mMember;
}

inline bool XPCCallContext::HasInterfaceAndMember() const {
  return mState >= HAVE_NAME && mInterface && mMember;
}

inline bool XPCCallContext::GetStaticMemberIsLocal() const {
  CHECK_STATE(HAVE_NAME);
  return mStaticMemberIsLocal;
}

inline unsigned XPCCallContext::GetArgc() const {
  CHECK_STATE(READY_TO_CALL);
  return mArgc;
}

inline JS::Value* XPCCallContext::GetArgv() const {
  CHECK_STATE(READY_TO_CALL);
  return mArgv;
}

inline void XPCCallContext::SetRetVal(const JS::Value& val) {
  CHECK_STATE(HAVE_ARGS);
  if (mRetVal) {
    *mRetVal = val;
  }
}

inline jsid XPCCallContext::GetResolveName() const {
  CHECK_STATE(HAVE_CONTEXT);
  return GetContext()->GetResolveName();
}

inline jsid XPCCallContext::SetResolveName(JS::HandleId name) {
  CHECK_STATE(HAVE_CONTEXT);
  return GetContext()->SetResolveName(name);
}

inline XPCWrappedNative* XPCCallContext::GetResolvingWrapper() const {
  CHECK_STATE(HAVE_OBJECT);
  return GetContext()->GetResolvingWrapper();
}

inline XPCWrappedNative* XPCCallContext::SetResolvingWrapper(
    XPCWrappedNative* w) {
  CHECK_STATE(HAVE_OBJECT);
  return GetContext()->SetResolvingWrapper(w);
}

inline uint16_t XPCCallContext::GetMethodIndex() const {
  CHECK_STATE(HAVE_OBJECT);
  return mMethodIndex;
}

/***************************************************************************/
inline XPCNativeInterface* XPCNativeMember::GetInterface() const {
  XPCNativeMember* arrayStart =
      const_cast<XPCNativeMember*>(this - mIndexInInterface);
  size_t arrayStartOffset = XPCNativeInterface::OffsetOfMembers();
  char* xpcNativeInterfaceStart =
      reinterpret_cast<char*>(arrayStart) - arrayStartOffset;
  return reinterpret_cast<XPCNativeInterface*>(xpcNativeInterfaceStart);
}

/***************************************************************************/

inline const nsIID* XPCNativeInterface::GetIID() const { return &mInfo->IID(); }

inline const char* XPCNativeInterface::GetNameString() const {
  return mInfo->Name();
}

inline XPCNativeMember* XPCNativeInterface::FindMember(jsid name) const {
  const XPCNativeMember* member = mMembers;
  for (int i = (int)mMemberCount; i > 0; i--, member++) {
    if (member->GetName() == name) {
      return const_cast<XPCNativeMember*>(member);
    }
  }
  return nullptr;
}

/* static */
inline size_t XPCNativeInterface::OffsetOfMembers() {
  return offsetof(XPCNativeInterface, mMembers);
}

/***************************************************************************/

inline XPCNativeSetKey::XPCNativeSetKey(XPCNativeSet* baseSet,
                                        XPCNativeInterface* addition)
    : mCx(nullptr), mBaseSet(baseSet), mAddition(addition) {
  MOZ_ASSERT(mBaseSet);
  MOZ_ASSERT(mAddition);
  MOZ_ASSERT(!mBaseSet->HasInterface(mAddition));
}

/***************************************************************************/

inline bool XPCNativeSet::FindMember(jsid name, XPCNativeMember** pMember,
                                     uint16_t* pInterfaceIndex) const {
  XPCNativeInterface* const* iface;
  int count = (int)mInterfaceCount;
  int i;

  // look for interface names first

  for (i = 0, iface = mInterfaces; i < count; i++, iface++) {
    if (name == (*iface)->GetName()) {
      if (pMember) {
        *pMember = nullptr;
      }
      if (pInterfaceIndex) {
        *pInterfaceIndex = (uint16_t)i;
      }
      return true;
    }
  }

  // look for method names
  for (i = 0, iface = mInterfaces; i < count; i++, iface++) {
    XPCNativeMember* member = (*iface)->FindMember(name);
    if (member) {
      if (pMember) {
        *pMember = member;
      }
      if (pInterfaceIndex) {
        *pInterfaceIndex = (uint16_t)i;
      }
      return true;
    }
  }
  return false;
}

inline bool XPCNativeSet::FindMember(
    jsid name, XPCNativeMember** pMember,
    RefPtr<XPCNativeInterface>* pInterface) const {
  uint16_t index;
  if (!FindMember(name, pMember, &index)) {
    return false;
  }
  *pInterface = mInterfaces[index];
  return true;
}

inline bool XPCNativeSet::FindMember(JS::HandleId name,
                                     XPCNativeMember** pMember,
                                     RefPtr<XPCNativeInterface>* pInterface,
                                     XPCNativeSet* protoSet,
                                     bool* pIsLocal) const {
  XPCNativeMember* Member;
  RefPtr<XPCNativeInterface> Interface;
  XPCNativeMember* protoMember;

  if (!FindMember(name, &Member, &Interface)) {
    return false;
  }

  *pMember = Member;

  *pIsLocal = !Member || !protoSet ||
              (protoSet != this &&
               !protoSet->MatchesSetUpToInterface(this, Interface) &&
               (!protoSet->FindMember(name, &protoMember, (uint16_t*)nullptr) ||
                protoMember != Member));

  *pInterface = std::move(Interface);

  return true;
}

inline bool XPCNativeSet::HasInterface(XPCNativeInterface* aInterface) const {
  XPCNativeInterface* const* pp = mInterfaces;

  for (int i = (int)mInterfaceCount; i > 0; i--, pp++) {
    if (aInterface == *pp) {
      return true;
    }
  }
  return false;
}

inline bool XPCNativeSet::MatchesSetUpToInterface(
    const XPCNativeSet* other, XPCNativeInterface* iface) const {
  int count = std::min(int(mInterfaceCount), int(other->mInterfaceCount));

  XPCNativeInterface* const* pp1 = mInterfaces;
  XPCNativeInterface* const* pp2 = other->mInterfaces;

  for (int i = (int)count; i > 0; i--, pp1++, pp2++) {
    XPCNativeInterface* cur = (*pp1);
    if (cur != (*pp2)) {
      return false;
    }
    if (cur == iface) {
      return true;
    }
  }
  return false;
}

/***************************************************************************/

inline JSObject* XPCWrappedNativeTearOff::GetJSObjectPreserveColor() const {
  return mJSObject.unbarrieredGetPtr();
}

inline JSObject* XPCWrappedNativeTearOff::GetJSObject() { return mJSObject; }

inline void XPCWrappedNativeTearOff::SetJSObject(JSObject* JSObj) {
  MOZ_ASSERT(!IsMarked());
  mJSObject = JSObj;
}

inline void XPCWrappedNativeTearOff::JSObjectMoved(JSObject* obj,
                                                   const JSObject* old) {
  MOZ_ASSERT(!IsMarked());
  MOZ_ASSERT(mJSObject == old);
  mJSObject = obj;
}

inline XPCWrappedNativeTearOff::~XPCWrappedNativeTearOff() {
  MOZ_COUNT_DTOR(XPCWrappedNativeTearOff);
  MOZ_ASSERT(!(GetInterface() || GetNative() || GetJSObjectPreserveColor()),
             "tearoff not empty in dtor");
}

/***************************************************************************/

inline void XPCWrappedNative::SweepTearOffs() {
  for (XPCWrappedNativeTearOff* to = &mFirstTearOff; to;
       to = to->GetNextTearOff()) {
    bool marked = to->IsMarked();
    to->Unmark();
    if (marked) {
      continue;
    }

    // If this tearoff does not have a live dedicated JSObject,
    // then let's recycle it.
    if (!to->GetJSObjectPreserveColor()) {
      to->SetNative(nullptr);
      to->SetInterface(nullptr);
    }
  }
}

/***************************************************************************/

inline bool xpc_ForcePropertyResolve(JSContext* cx, JS::HandleObject obj,
                                     jsid idArg) {
  JS::RootedId id(cx, idArg);
  bool dummy;
  return JS_HasPropertyById(cx, obj, id, &dummy);
}

inline jsid GetJSIDByIndex(JSContext* cx, unsigned index) {
  XPCJSRuntime* xpcrt = nsXPConnect::GetRuntimeInstance();
  return xpcrt->GetStringID(index);
}

inline bool ThrowBadParam(nsresult rv, unsigned paramNum, XPCCallContext& ccx) {
  XPCThrower::ThrowBadParam(rv, paramNum, ccx);
  return false;
}

inline void ThrowBadResult(nsresult result, XPCCallContext& ccx) {
  XPCThrower::ThrowBadResult(NS_ERROR_XPC_NATIVE_RETURNED_FAILURE, result, ccx);
}

/***************************************************************************/

inline void xpc::CleanupValue(const nsXPTType& aType, void* aValue,
                              uint32_t aArrayLen) {
  // Check if we can do a cheap early return, and only perform the inner call
  // if we can't. We never have to clean up null pointer types or arithmetic
  // types.
  //
  // NOTE: We can skip zeroing arithmetic types in CleanupValue, as they are
  // already in a valid state.
  if (aType.IsArithmetic() || (aType.IsPointer() && !*(void**)aValue)) {
    return;
  }
  xpc::InnerCleanupValue(aType, aValue, aArrayLen);
}

/***************************************************************************/

#endif /* xpcinlines_h___ */
