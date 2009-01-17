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

class XPCNativeWrapper
{
public:
  static PRBool AttachNewConstructorObject(XPCCallContext &ccx,
                                           JSObject *aGlobalObject);

  static JSObject *GetNewOrUsed(JSContext *cx, XPCWrappedNative *wrapper,
                                nsIPrincipal *aObjectPrincipal);

  static PRBool IsNativeWrapperClass(JSClass *clazz)
  {
    return clazz == &sXPC_NW_JSClass.base;
  }

  static PRBool IsNativeWrapper(JSObject *obj)
  {
    return STOBJ_GET_CLASS(obj) == &sXPC_NW_JSClass.base;
  }

  static JSBool GetWrappedNative(JSContext *cx, JSObject *obj,
                                 XPCWrappedNative **aWrappedNative);

  // NB: Use the following carefully.
  static XPCWrappedNative *SafeGetWrappedNative(JSObject *obj)
  {
      return static_cast<XPCWrappedNative *>(xpc_GetJSPrivate(obj));
  }


  static JSClass *GetJSClass()
  {
    return &sXPC_NW_JSClass.base;
  }

  static void ClearWrappedNativeScopes(JSContext* cx,
                                       XPCWrappedNative* wrapper);

protected:
  static JSExtendedClass sXPC_NW_JSClass;
};

JSBool
XPC_XOW_WrapObject(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                   jsval *rval);
