/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include "nscore.h"
#include "jsapi.h"

class nsIPrincipal;

namespace XPCNativeWrapper {

namespace internal {
  extern JSExtendedClass NW_NoCall_Class;
  extern JSExtendedClass NW_Call_Class;
}

PRBool
AttachNewConstructorObject(XPCCallContext &ccx, JSObject *aGlobalObject);

JSObject *
GetNewOrUsed(JSContext *cx, XPCWrappedNative *wrapper,
             JSObject *scope, nsIPrincipal *aObjectPrincipal);
JSBool
CreateExplicitWrapper(JSContext *cx, XPCWrappedNative *wrapper, jsval *rval);

inline PRBool
IsNativeWrapperClass(JSClass *clazz)
{
  return clazz == &internal::NW_NoCall_Class.base ||
         clazz == &internal::NW_Call_Class.base;
}

inline PRBool
IsNativeWrapper(JSObject *obj)
{
  return IsNativeWrapperClass(obj->getClass());
}

JSBool
GetWrappedNative(JSContext *cx, JSObject *obj,
                 XPCWrappedNative **aWrappedNative);

// NB: Use the following carefully.
inline XPCWrappedNative *
SafeGetWrappedNative(JSObject *obj)
{
  return static_cast<XPCWrappedNative *>(xpc_GetJSPrivate(obj));
}

inline JSClass *
GetJSClass(bool call)
{
  return call
    ? &internal::NW_Call_Class.base
    : &internal::NW_NoCall_Class.base;
}

void
ClearWrappedNativeScopes(JSContext* cx, XPCWrappedNative* wrapper);

}
