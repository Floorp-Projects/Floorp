/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
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
 * The Original Code is js-ctypes.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation <http://www.mozilla.org/>.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Mark Finkle <mark.finkle@gmail.com>, <mfinkle@mozilla.com>
 *  Dan Witte <dwitte@mozilla.com>
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

#include "Module.h"
#include "Library.h"
#include "nsIGenericFactory.h"
#include "nsMemory.h"

#define JSCTYPES_CONTRACTID \
  "@mozilla.org/jsctypes;1"

#define JSCTYPES_CID \
{ 0xc797702, 0x1c60, 0x4051, { 0x9d, 0xd7, 0x4d, 0x74, 0x5, 0x60, 0x56, 0x42 } }

namespace mozilla {
namespace ctypes {

NS_GENERIC_FACTORY_CONSTRUCTOR(Module)

NS_IMPL_ISUPPORTS1(Module, nsIXPCScriptable)

Module::Module()
{
}

Module::~Module()
{
}

#define XPC_MAP_CLASSNAME Module
#define XPC_MAP_QUOTED_CLASSNAME "Module"
#define XPC_MAP_WANT_CALL
#define XPC_MAP_FLAGS nsIXPCScriptable::WANT_CALL
#include "xpc_map_end.h"

NS_IMETHODIMP
Module::Call(nsIXPConnectWrappedNative* wrapper,
             JSContext* cx,
             JSObject* obj,
             PRUint32 argc,
             jsval* argv,
             jsval* vp,
             PRBool* _retval)
{
  JSObject* global = JS_GetGlobalObject(cx);
  *_retval = Init(cx, global);
  return NS_OK;
}

static JSClass sCABIClass = {
  "CABI",
  JSCLASS_HAS_RESERVED_SLOTS(1),
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
  JS_EnumerateStub,JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

static JSClass sCTypeClass = {
  "CType",
  JSCLASS_HAS_RESERVED_SLOTS(1),
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
  JS_EnumerateStub,JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

static JSFunctionSpec sModuleFunctions[] = {
  JS_FN("open", Library::Open, 0, JSFUN_FAST_NATIVE | JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
  JS_FS_END
};

ABICode
Module::GetABICode(JSContext* cx, jsval val)
{
  // make sure we have an object representing a CABI class,
  // and extract the enumerated class type from the reserved slot.
  if (!JSVAL_IS_OBJECT(val))
    return INVALID_ABI;

  JSObject* obj = JSVAL_TO_OBJECT(val);
  if (JS_GET_CLASS(cx, obj) != &sCABIClass)
    return INVALID_ABI;

  jsval result;
  JS_GetReservedSlot(cx, obj, 0, &result);

  return ABICode(JSVAL_TO_INT(result));
}

TypeCode
Module::GetTypeCode(JSContext* cx, jsval val)
{
  // make sure we have an object representing a CType class,
  // and extract the enumerated class type from the reserved slot.
  if (!JSVAL_IS_OBJECT(val))
    return INVALID_TYPE;

  JSObject* obj = JSVAL_TO_OBJECT(val);
  if (JS_GET_CLASS(cx, obj) != &sCTypeClass)
    return INVALID_TYPE;

  jsval result;
  JS_GetReservedSlot(cx, obj, 0, &result);

  return TypeCode(JSVAL_TO_INT(result));
}

static bool
DefineConstant(JSContext* cx, JSObject* parent, JSClass* clasp, const char* name, jsint code)
{
  JSObject* obj = JS_DefineObject(cx, parent, name, clasp, NULL,
                    JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT);
  if (!obj)
    return false;

  return JS_SetReservedSlot(cx, obj, 0, INT_TO_JSVAL(code));
}

bool
Module::Init(JSContext* cx, JSObject* aGlobal)
{
  // attach ctypes property to global object
  JSObject* ctypes = JS_NewObject(cx, NULL, NULL, NULL);
  if (!ctypes)
    return false;

  if (!JS_DefineProperty(cx, aGlobal, "ctypes", OBJECT_TO_JSVAL(ctypes),
         NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT))
    return false;

  // attach classes representing ABI and type constants
#define DEFINE_ABI(name)                                                \
  if (!DefineConstant(cx, ctypes, &sCABIClass, #name, ABI_##name))      \
    return false;
#define DEFINE_TYPE(name)                                               \
  if (!DefineConstant(cx, ctypes, &sCTypeClass, #name, TYPE_##name))    \
    return false;
#include "types.h"
#undef DEFINE_ABI
#undef DEFINE_TYPE

  // attach API functions
  if (!JS_DefineFunctions(cx, ctypes, sModuleFunctions))
    return false;

  return true;
}

}
}

static nsModuleComponentInfo components[] =
{
  {
    "jsctypes",
    JSCTYPES_CID,
    JSCTYPES_CONTRACTID,
    mozilla::ctypes::ModuleConstructor,
  }
};

NS_IMPL_NSGETMODULE(jsctypes, components)

