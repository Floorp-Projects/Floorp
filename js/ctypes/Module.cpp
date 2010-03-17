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
#include "CTypes.h"
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

#define CTYPESFN_FLAGS \
  (JSFUN_FAST_NATIVE | JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT)

static JSFunctionSpec sModuleFunctions[] = {
  JS_FN("open", Library::Open, 1, CTYPESFN_FLAGS),
  JS_FN("cast", CData::Cast, 2, CTYPESFN_FLAGS),
  JS_FS_END
};

static JSBool
SealObjectAndPrototype(JSContext* cx, JSObject* parent, const char* name)
{
  jsval prop;
  if (!JS_GetProperty(cx, parent, name, &prop))
    return false;

  JSObject* obj = JSVAL_TO_OBJECT(prop);
  if (!JS_GetProperty(cx, obj, "prototype", &prop))
    return false;

  JSObject* prototype = JSVAL_TO_OBJECT(prop);
  return JS_SealObject(cx, obj, JS_FALSE) &&
         JS_SealObject(cx, prototype, JS_FALSE);
}

JSBool
Module::Init(JSContext* cx, JSObject* aGlobal)
{
  // attach ctypes property to global object
  JSObject* ctypes = JS_NewObject(cx, NULL, NULL, NULL);
  if (!ctypes)
    return false;

  if (!JS_DefineProperty(cx, aGlobal, "ctypes", OBJECT_TO_JSVAL(ctypes),
         NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT))
    return false;

  if (!InitTypeClasses(cx, ctypes))
    return false;

  // attach API functions
  if (!JS_DefineFunctions(cx, ctypes, sModuleFunctions))
    return false;

  // Seal the ctypes object, to prevent modification. (This single object
  // instance is shared amongst everyone who imports the ctypes module.)
  if (!JS_SealObject(cx, ctypes, JS_FALSE))
    return false;

  // Seal up Object, Function, and Array and their prototypes.
  if (!SealObjectAndPrototype(cx, aGlobal, "Object") ||
      !SealObjectAndPrototype(cx, aGlobal, "Function") ||
      !SealObjectAndPrototype(cx, aGlobal, "Array"))
    return false;

  // Finally, seal the global object, for good measure. (But not recursively;
  // this breaks things.)
  return JS_SealObject(cx, aGlobal, JS_FALSE);
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

