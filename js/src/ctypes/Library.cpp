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
 *  Fredrik Larsson <nossralf@gmail.com>
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

#include "jscntxt.h"
#include "Library.h"
#include "CTypes.h"
#include "prlink.h"

namespace js {
namespace ctypes {

/*******************************************************************************
** JSAPI function prototypes
*******************************************************************************/

namespace Library
{
  static void Finalize(JSContext* cx, JSObject* obj);

  static JSBool Close(JSContext* cx, uintN argc, jsval* vp);
  static JSBool Declare(JSContext* cx, uintN argc, jsval* vp);
}

/*******************************************************************************
** JSObject implementation
*******************************************************************************/

static JSClass sLibraryClass = {
  "Library",
  JSCLASS_HAS_RESERVED_SLOTS(LIBRARY_SLOTS) | JSCLASS_MARK_IS_TRACE,
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
  JS_EnumerateStub,JS_ResolveStub, JS_ConvertStub, Library::Finalize,
  JSCLASS_NO_OPTIONAL_MEMBERS
};

#define CTYPESFN_FLAGS \
  (JSFUN_FAST_NATIVE | JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT)

static JSFunctionSpec sLibraryFunctions[] = {
  JS_FN("close",   Library::Close,   0, CTYPESFN_FLAGS),
  JS_FN("declare", Library::Declare, 0, CTYPESFN_FLAGS),
  JS_FS_END
};

JSObject*
Library::Create(JSContext* cx, jsval aPath)
{
  JSObject* libraryObj = JS_NewObject(cx, &sLibraryClass, NULL, NULL);
  if (!libraryObj)
    return NULL;
  js::AutoValueRooter root(cx, libraryObj);

  // initialize the library
  if (!JS_SetReservedSlot(cx, libraryObj, SLOT_LIBRARY, PRIVATE_TO_JSVAL(NULL)))
    return NULL;

  // attach API functions
  if (!JS_DefineFunctions(cx, libraryObj, sLibraryFunctions))
    return NULL;

  if (!JSVAL_IS_STRING(aPath)) {
    JS_ReportError(cx, "open takes a string argument");
    return NULL;
  }

  PRLibSpec libSpec;
#ifdef XP_WIN
  // On Windows, converting to native charset may corrupt path string.
  // So, we have to use Unicode path directly.
  const PRUnichar* path = reinterpret_cast<const PRUnichar*>(
    JS_GetStringCharsZ(cx, JSVAL_TO_STRING(aPath)));
  if (!path)
    return NULL;

  libSpec.value.pathname_u = path;
  libSpec.type = PR_LibSpec_PathnameU;
#else
  // Assume the JS string is not UTF-16, but is in the platform's native
  // charset. (This basically means ASCII.) It would be nice to have a
  // UTF-16 -> native charset implementation available. :(
  const char* path = JS_GetStringBytesZ(cx, JSVAL_TO_STRING(aPath));
  if (!path)
    return NULL;

  libSpec.value.pathname = path;
  libSpec.type = PR_LibSpec_Pathname;
#endif

  PRLibrary* library = PR_LoadLibraryWithFlags(libSpec, 0);
  if (!library) {
    JS_ReportError(cx, "couldn't open library");
    return NULL;
  }

  // stash the library
  if (!JS_SetReservedSlot(cx, libraryObj, SLOT_LIBRARY,
         PRIVATE_TO_JSVAL(library)))
    return NULL;

  return libraryObj;
}

bool
Library::IsLibrary(JSContext* cx, JSObject* obj)
{
  return JS_GET_CLASS(cx, obj) == &sLibraryClass;
}

PRLibrary*
Library::GetLibrary(JSContext* cx, JSObject* obj)
{
  JS_ASSERT(IsLibrary(cx, obj));

  jsval slot;
  JS_GetReservedSlot(cx, obj, SLOT_LIBRARY, &slot);
  return static_cast<PRLibrary*>(JSVAL_TO_PRIVATE(slot));
}

void
Library::Finalize(JSContext* cx, JSObject* obj)
{
  // unload the library
  PRLibrary* library = GetLibrary(cx, obj);
  if (library)
    PR_UnloadLibrary(library);
}

JSBool
Library::Open(JSContext* cx, uintN argc, jsval *vp)
{
  if (argc != 1 || JSVAL_IS_VOID(JS_ARGV(cx, vp)[0])) {
    JS_ReportError(cx, "open requires a single argument");
    return JS_FALSE;
  }

  JSObject* library = Create(cx, JS_ARGV(cx, vp)[0]);
  if (!library)
    return JS_FALSE;

  JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(library));
  return JS_TRUE;
}

JSBool
Library::Close(JSContext* cx, uintN argc, jsval* vp)
{
  JSObject* obj = JS_THIS_OBJECT(cx, vp);
  if (!IsLibrary(cx, obj)) {
    JS_ReportError(cx, "not a library");
    return JS_FALSE;
  }

  if (argc != 0) {
    JS_ReportError(cx, "close doesn't take any arguments");
    return JS_FALSE;
  }

  // delete our internal objects
  Finalize(cx, obj);
  JS_SetReservedSlot(cx, obj, SLOT_LIBRARY, PRIVATE_TO_JSVAL(NULL));

  JS_SET_RVAL(cx, vp, JSVAL_VOID);
  return JS_TRUE;
}

JSBool
Library::Declare(JSContext* cx, uintN argc, jsval* vp)
{
  JSObject* obj = JS_THIS_OBJECT(cx, vp);
  if (!IsLibrary(cx, obj)) {
    JS_ReportError(cx, "not a library");
    return JS_FALSE;
  }

  PRLibrary* library = GetLibrary(cx, obj);
  if (!library) {
    JS_ReportError(cx, "library not open");
    return JS_FALSE;
  }

  // We allow two API variants:
  // 1) library.declare(name, abi, returnType, argType1, ...)
  //    declares a function with the given properties, and resolves the symbol
  //    address in the library.
  // 2) library.declare(name, type)
  //    declares a symbol of 'type', and resolves it. The object that comes
  //    back will be of type 'type', and will point into the symbol data.
  //    This data will be both readable and writable via the usual CData
  //    accessors. If 'type' is a FunctionType, the result will be a function
  //    pointer, as with 1). 
  if (argc < 2) {
    JS_ReportError(cx, "declare requires at least two arguments");
    return JS_FALSE;
  }

  jsval* argv = JS_ARGV(cx, vp);
  if (!JSVAL_IS_STRING(argv[0])) {
    JS_ReportError(cx, "first argument must be a string");
    return JS_FALSE;
  }

  const char* name = JS_GetStringBytesZ(cx, JSVAL_TO_STRING(argv[0]));
  if (!name)
    return JS_FALSE;

  JSObject* typeObj;
  js::AutoValueRooter root(cx);
  bool isFunction = argc > 2;
  if (isFunction) {
    // Case 1).
    // Create a FunctionType representing the function.
    typeObj = FunctionType::CreateInternal(cx,
                argv[1], argv[2], &argv[3], argc - 3);
    if (!typeObj)
      return JS_FALSE;
    root.setObject(typeObj);

    // Make a function pointer type.
    typeObj = PointerType::CreateInternal(cx, typeObj);
    if (!typeObj)
      return JS_FALSE;
    root.setObject(typeObj);

  } else {
    // Case 2).
    if (JSVAL_IS_PRIMITIVE(argv[1]) ||
        !CType::IsCType(cx, JSVAL_TO_OBJECT(argv[1])) ||
        !CType::IsSizeDefined(cx, JSVAL_TO_OBJECT(argv[1]))) {
      JS_ReportError(cx, "second argument must be a type of defined size");
      return JS_FALSE;
    }

    typeObj = JSVAL_TO_OBJECT(argv[1]);
    if (CType::GetTypeCode(cx, typeObj) == TYPE_pointer) {
      JSObject* baseType = PointerType::GetBaseType(cx, typeObj);
      isFunction = baseType && CType::GetTypeCode(cx, baseType) == TYPE_function;
    }
  }

  void* data;
  PRFuncPtr fnptr;
  if (isFunction) {
    // Look up the function symbol.
    fnptr = PR_FindFunctionSymbol(library, name);
    if (!fnptr) {
      JS_ReportError(cx, "couldn't find function symbol in library");
      return JS_FALSE;
    }
    data = &fnptr;

  } else {
    // 'typeObj' is another data type. Look up the data symbol.
    data = PR_FindSymbol(library, name);
    if (!data) {
      JS_ReportError(cx, "couldn't find symbol in library");
      return JS_FALSE;
    }
  }

  JSObject* result = CData::Create(cx, typeObj, obj, data, isFunction);
  if (!result)
    return JS_FALSE;

  JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(result));

  // Seal the CData object, to prevent modification of the function pointer.
  // This permanently associates this object with the library, and avoids
  // having to do things like reset SLOT_REFERENT when someone tries to
  // change the pointer value.
  // XXX This will need to change when bug 541212 is fixed -- CData::ValueSetter
  // could be called on a sealed object.
  if (isFunction && !JS_SealObject(cx, result, JS_FALSE))
    return JS_FALSE;

  return JS_TRUE;
}

}
}

