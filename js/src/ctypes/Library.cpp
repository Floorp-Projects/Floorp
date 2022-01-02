/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ctypes/Library.h"

#include "jsapi.h"
#include "prerror.h"
#include "prlink.h"

#include "ctypes/CTypes.h"
#include "js/CharacterEncoding.h"
#include "js/ErrorReport.h"
#include "js/experimental/CTypes.h"  // JS::CTypesCallbacks
#include "js/MemoryFunctions.h"
#include "js/Object.h"              // JS::GetReservedSlot
#include "js/PropertyAndElement.h"  // JS_DefineFunctions
#include "js/PropertySpec.h"
#include "js/StableStringChars.h"
#include "js/ValueArray.h"
#include "vm/JSObject.h"

using JS::AutoStableStringChars;

namespace js::ctypes {

/*******************************************************************************
** JSAPI function prototypes
*******************************************************************************/

namespace Library {
static void Finalize(JSFreeOp* fop, JSObject* obj);

static bool Close(JSContext* cx, unsigned argc, Value* vp);
static bool Declare(JSContext* cx, unsigned argc, Value* vp);
}  // namespace Library

/*******************************************************************************
** JSObject implementation
*******************************************************************************/

static const JSClassOps sLibraryClassOps = {
    nullptr,            // addProperty
    nullptr,            // delProperty
    nullptr,            // enumerate
    nullptr,            // newEnumerate
    nullptr,            // resolve
    nullptr,            // mayResolve
    Library::Finalize,  // finalize
    nullptr,            // call
    nullptr,            // hasInstance
    nullptr,            // construct
    nullptr,            // trace
};

static const JSClass sLibraryClass = {
    "Library",
    JSCLASS_HAS_RESERVED_SLOTS(LIBRARY_SLOTS) | JSCLASS_FOREGROUND_FINALIZE,
    &sLibraryClassOps};

#define CTYPESFN_FLAGS (JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT)

static const JSFunctionSpec sLibraryFunctions[] = {
    JS_FN("close", Library::Close, 0, CTYPESFN_FLAGS),
    JS_FN("declare", Library::Declare, 0, CTYPESFN_FLAGS), JS_FS_END};

bool Library::Name(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() != 1) {
    JS_ReportErrorASCII(cx, "libraryName takes one argument");
    return false;
  }

  Value arg = args[0];
  JSString* str = nullptr;
  if (arg.isString()) {
    str = arg.toString();
  } else {
    JS_ReportErrorASCII(cx, "name argument must be a string");
    return false;
  }

  AutoString resultString;
  AppendString(cx, resultString, MOZ_DLL_PREFIX);
  AppendString(cx, resultString, str);
  AppendString(cx, resultString, MOZ_DLL_SUFFIX);
  if (!resultString) {
    return false;
  }
  auto resultStr = resultString.finish();

  JSString* result =
      JS_NewUCStringCopyN(cx, resultStr.begin(), resultStr.length());
  if (!result) {
    return false;
  }

  args.rval().setString(result);
  return true;
}

JSObject* Library::Create(JSContext* cx, HandleValue path,
                          const JS::CTypesCallbacks* callbacks) {
  RootedObject libraryObj(cx, JS_NewObject(cx, &sLibraryClass));
  if (!libraryObj) {
    return nullptr;
  }

  // initialize the library
  JS_SetReservedSlot(libraryObj, SLOT_LIBRARY, PrivateValue(nullptr));

  // attach API functions
  if (!JS_DefineFunctions(cx, libraryObj, sLibraryFunctions)) {
    return nullptr;
  }

  if (!path.isString()) {
    JS_ReportErrorASCII(cx, "open takes a string argument");
    return nullptr;
  }

  PRLibSpec libSpec;
  RootedLinearString pathStr(cx, JS_EnsureLinearString(cx, path.toString()));
  if (!pathStr) {
    return nullptr;
  }
#ifdef XP_WIN
  // On Windows, converting to native charset may corrupt path string.
  // So, we have to use Unicode path directly.
  JS::UniqueTwoByteChars pathZeroTerminated(JS_CopyStringCharsZ(cx, pathStr));
  if (!pathZeroTerminated) {
    return nullptr;
  }
  char16ptr_t pathChars = pathZeroTerminated.get();
  libSpec.value.pathname_u = pathChars;
  libSpec.type = PR_LibSpec_PathnameU;
#else
  // Convert to platform native charset if the appropriate callback has been
  // provided.
  JS::UniqueChars pathBytes;
  if (callbacks && callbacks->unicodeToNative) {
    AutoStableStringChars pathStrChars(cx);
    if (!pathStrChars.initTwoByte(cx, pathStr)) {
      return nullptr;
    }

    pathBytes.reset(callbacks->unicodeToNative(cx, pathStrChars.twoByteChars(),
                                               pathStr->length()));
    if (!pathBytes) {
      return nullptr;
    }
  } else {
    // Fallback: assume the platform native charset is UTF-8. This is true
    // for Mac OS X, Android, and probably Linux.
    if (!ReportErrorIfUnpairedSurrogatePresent(cx, pathStr)) {
      return nullptr;
    }

    size_t nbytes = JS::GetDeflatedUTF8StringLength(pathStr);

    pathBytes.reset(static_cast<char*>(JS_malloc(cx, nbytes + 1)));
    if (!pathBytes) {
      return nullptr;
    }

    nbytes = JS::DeflateStringToUTF8Buffer(
        pathStr, mozilla::Span(pathBytes.get(), nbytes));
    pathBytes[nbytes] = 0;
  }

  libSpec.value.pathname = pathBytes.get();
  libSpec.type = PR_LibSpec_Pathname;
#endif

  PRLibrary* library = PR_LoadLibraryWithFlags(libSpec, PR_LD_NOW);

  if (!library) {
#define MAX_ERROR_LEN 1024
    char error[MAX_ERROR_LEN] = "Cannot get error from NSPR.";
    uint32_t errorLen = PR_GetErrorTextLength();
    if (errorLen && errorLen < MAX_ERROR_LEN) {
      PR_GetErrorText(error);
    }
#undef MAX_ERROR_LEN

    if (JS::StringIsASCII(error)) {
      if (JS::UniqueChars pathCharsUTF8 = JS_EncodeStringToUTF8(cx, pathStr)) {
        JS_ReportErrorUTF8(cx, "couldn't open library %s: %s",
                           pathCharsUTF8.get(), error);
      }
    } else {
      if (JS::UniqueChars pathCharsLatin1 =
              JS_EncodeStringToLatin1(cx, pathStr)) {
        JS_ReportErrorLatin1(cx, "couldn't open library %s: %s",
                             pathCharsLatin1.get(), error);
      }
    }
    return nullptr;
  }

  // stash the library
  JS_SetReservedSlot(libraryObj, SLOT_LIBRARY, PrivateValue(library));

  return libraryObj;
}

bool Library::IsLibrary(JSObject* obj) { return obj->hasClass(&sLibraryClass); }

PRLibrary* Library::GetLibrary(JSObject* obj) {
  MOZ_ASSERT(IsLibrary(obj));

  Value slot = JS::GetReservedSlot(obj, SLOT_LIBRARY);
  return static_cast<PRLibrary*>(slot.toPrivate());
}

static void UnloadLibrary(JSObject* obj) {
  PRLibrary* library = Library::GetLibrary(obj);
  if (library) {
    PR_UnloadLibrary(library);
  }
}

void Library::Finalize(JSFreeOp* fop, JSObject* obj) { UnloadLibrary(obj); }

bool Library::Open(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  JSObject* ctypesObj = GetThisObject(cx, args, "ctypes.open");
  if (!ctypesObj) {
    return false;
  }

  if (!IsCTypesGlobal(ctypesObj)) {
    JS_ReportErrorASCII(cx, "not a ctypes object");
    return false;
  }

  if (args.length() != 1 || args[0].isUndefined()) {
    JS_ReportErrorASCII(cx, "open requires a single argument");
    return false;
  }

  JSObject* library = Create(cx, args[0], GetCallbacks(ctypesObj));
  if (!library) {
    return false;
  }

  args.rval().setObject(*library);
  return true;
}

bool Library::Close(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  RootedObject obj(cx, GetThisObject(cx, args, "ctypes.close"));
  if (!obj) {
    return false;
  }

  if (!IsLibrary(obj)) {
    JS_ReportErrorASCII(cx, "not a library");
    return false;
  }

  if (args.length() != 0) {
    JS_ReportErrorASCII(cx, "close doesn't take any arguments");
    return false;
  }

  // delete our internal objects
  UnloadLibrary(obj);
  JS_SetReservedSlot(obj, SLOT_LIBRARY, PrivateValue(nullptr));

  args.rval().setUndefined();
  return true;
}

bool Library::Declare(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  RootedObject obj(cx, GetThisObject(cx, args, "ctypes.declare"));
  if (!obj) {
    return false;
  }

  if (!IsLibrary(obj)) {
    JS_ReportErrorASCII(cx, "not a library");
    return false;
  }

  PRLibrary* library = GetLibrary(obj);
  if (!library) {
    JS_ReportErrorASCII(cx, "library not open");
    return false;
  }

  // We allow two API variants:
  // 1) library.declare(name, abi, returnType, argType1, ...)
  //    declares a function with the given properties, and resolves the symbol
  //    address in the library.
  // 2) library.declare(name, type)
  //    declares a symbol of 'type', and resolves it. The object that comes
  //    back will be of type 'type', and will point into the symbol data.
  //    This data will be both readable and writable via the usual CData
  //    accessors. If 'type' is a PointerType to a FunctionType, the result will
  //    be a function pointer, as with 1).
  if (args.length() < 2) {
    JS_ReportErrorASCII(cx, "declare requires at least two arguments");
    return false;
  }

  if (!args[0].isString()) {
    JS_ReportErrorASCII(cx, "first argument must be a string");
    return false;
  }

  RootedObject fnObj(cx, nullptr);
  RootedObject typeObj(cx);
  bool isFunction = args.length() > 2;
  if (isFunction) {
    // Case 1).
    // Create a FunctionType representing the function.
    fnObj = FunctionType::CreateInternal(
        cx, args[1], args[2],
        HandleValueArray::subarray(args, 3, args.length() - 3));
    if (!fnObj) {
      return false;
    }

    // Make a function pointer type.
    typeObj = PointerType::CreateInternal(cx, fnObj);
    if (!typeObj) {
      return false;
    }
  } else {
    // Case 2).
    if (args[1].isPrimitive() || !CType::IsCType(args[1].toObjectOrNull()) ||
        !CType::IsSizeDefined(args[1].toObjectOrNull())) {
      JS_ReportErrorASCII(cx, "second argument must be a type of defined size");
      return false;
    }

    typeObj = args[1].toObjectOrNull();
    if (CType::GetTypeCode(typeObj) == TYPE_pointer) {
      fnObj = PointerType::GetBaseType(typeObj);
      isFunction = fnObj && CType::GetTypeCode(fnObj) == TYPE_function;
    }
  }

  void* data;
  PRFuncPtr fnptr;
  RootedString nameStr(cx, args[0].toString());
  AutoCString symbol;
  if (isFunction) {
    // Build the symbol, with mangling if necessary.
    FunctionType::BuildSymbolName(cx, nameStr, fnObj, symbol);
    AppendString(cx, symbol, "\0");
    if (!symbol) {
      return false;
    }

    // Look up the function symbol.
    fnptr = PR_FindFunctionSymbol(library, symbol.finish().begin());
    if (!fnptr) {
      JS_ReportErrorASCII(cx, "couldn't find function symbol in library");
      return false;
    }
    data = &fnptr;

  } else {
    // 'typeObj' is another data type. Look up the data symbol.
    AppendString(cx, symbol, nameStr);
    AppendString(cx, symbol, "\0");
    if (!symbol) {
      return false;
    }

    data = PR_FindSymbol(library, symbol.finish().begin());
    if (!data) {
      JS_ReportErrorASCII(cx, "couldn't find symbol in library");
      return false;
    }
  }

  RootedObject result(cx, CData::Create(cx, typeObj, obj, data, isFunction));
  if (!result) {
    return false;
  }

  if (isFunction) {
    JS_SetReservedSlot(result, SLOT_FUNNAME, StringValue(nameStr));
  }

  args.rval().setObject(*result);

  // Seal the CData object, to prevent modification of the function pointer.
  // This permanently associates this object with the library, and avoids
  // having to do things like reset SLOT_REFERENT when someone tries to
  // change the pointer value.
  // XXX This will need to change when bug 541212 is fixed -- CData::ValueSetter
  // could be called on a sealed object.
  if (isFunction && !JS_FreezeObject(cx, result)) {
    return false;
  }

  return true;
}

}  // namespace js::ctypes
