/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XPC_WRAPPER_H
#define XPC_WRAPPER_H 1

#include "xpcprivate.h"
#include "jswrapper.h"

class nsIScriptSecurityManager;

namespace XPCNativeWrapper {

// Given an XPCWrappedNative pointer and the name of the function on
// XPCNativeScriptableFlags corresponding with a flag, returns 'true'
// if the flag is set.
// XXX Convert to using GetFlags() and not a macro.
#define NATIVE_HAS_FLAG(_wn, _flag)                                           \
  ((_wn)->GetScriptableInfo() &&                                              \
   (_wn)->GetScriptableInfo()->GetFlags()._flag())

bool
AttachNewConstructorObject(JSContext *aCx, JS::HandleObject aGlobalObject);

} // namespace XPCNativeWrapper

// This namespace wraps some common functionality between the three existing
// wrappers. Its main purpose is to allow XPCCrossOriginWrapper to act both
// as an XPCSafeJSObjectWrapper and as an XPCNativeWrapper when required to
// do so (the decision is based on the principals of the wrapper and wrapped
// objects).
namespace XPCWrapper {

JSObject *
UnsafeUnwrapSecurityWrapper(JSObject *obj);

} // namespace XPCWrapper


#endif
