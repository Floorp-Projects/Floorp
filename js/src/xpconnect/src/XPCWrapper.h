/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Johnny Stenback <jst@mozilla.org> (original author)
 *   Brendan Eich <brendan@mozilla.org>
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *   Blake Kaplan <mrbkap@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef XPC_WRAPPER_H
#define XPC_WRAPPER_H 1

#include "xpcprivate.h"

namespace XPCNativeWrapper {

// Given an XPCWrappedNative pointer and the name of the function on
// XPCNativeScriptableFlags corresponding with a flag, returns 'true'
// if the flag is set.
// XXX Convert to using GetFlags() and not a macro.
#define NATIVE_HAS_FLAG(_wn, _flag)                \
  ((_wn)->GetScriptableInfo() &&                   \
   (_wn)->GetScriptableInfo()->GetFlags()._flag())

bool
AttachNewConstructorObject(XPCCallContext &ccx, JSObject *aGlobalObject);

} // namespace XPCNativeWrapper

// This namespace wraps some common functionality between the three existing
// wrappers. Its main purpose is to allow XPCCrossOriginWrapper to act both
// as an XPCSafeJSObjectWrapper and as an XPCNativeWrapper when required to
// do so (the decision is based on the principals of the wrapper and wrapped
// objects).
namespace XPCWrapper {

/**
 * Returns the script security manager used by XPConnect.
 */
inline nsIScriptSecurityManager *
GetSecurityManager()
{
  return nsXPConnect::gScriptSecurityManager;
}

inline JSBool
IsSecurityWrapper(JSObject *wrapper)
{
  return wrapper->isWrapper();
}

/**
 * Given an arbitrary object, Unwrap will return the wrapped object if the
 * passed-in object is a wrapper that Unwrap knows about *and* the
 * currently running code has permission to access both the wrapper and
 * wrapped object.
 *
 * Since this is meant to be called from functions like
 * XPCWrappedNative::GetWrappedNativeOfJSObject, it does not set an
 * exception on |cx|.
 */
JSObject *
Unwrap(JSContext *cx, JSObject *wrapper);

JSObject *
UnsafeUnwrapSecurityWrapper(JSObject *obj);

} // namespace XPCWrapper


#endif
