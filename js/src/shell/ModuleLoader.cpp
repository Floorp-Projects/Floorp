/* -*- Mode: javascript; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4
 * -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "shell/ModuleLoader.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/TextUtils.h"

#include "NamespaceImports.h"

#include "js/MapAndSet.h"
#include "js/Modules.h"
#include "js/PropertyAndElement.h"  // JS_DefineProperty, JS_GetProperty
#include "js/SourceText.h"
#include "js/StableStringChars.h"
#include "shell/jsshell.h"
#include "shell/OSObject.h"
#include "shell/StringUtils.h"
#include "util/Text.h"
#include "vm/JSAtom.h"
#include "vm/JSContext.h"
#include "vm/StringType.h"

using namespace js;
using namespace js::shell;

static constexpr char16_t JavaScriptScheme[] = u"javascript:";

static bool IsJavaScriptURL(HandleLinearString path) {
  return StringStartsWith(path, JavaScriptScheme);
}

static JSString* ExtractJavaScriptURLSource(JSContext* cx,
                                            HandleLinearString path) {
  MOZ_ASSERT(IsJavaScriptURL(path));

  const size_t schemeLength = js_strlen(JavaScriptScheme);
  return SubString(cx, path, schemeLength);
}

bool ModuleLoader::init(JSContext* cx, HandleString loadPath) {
  loadPathStr = AtomizeString(cx, loadPath, PinAtom);
  if (!loadPathStr) {
    return false;
  }

  MOZ_ASSERT(IsAbsolutePath(loadPathStr));

  char16_t sep = PathSeparator;
  pathSeparatorStr = AtomizeChars(cx, &sep, 1);
  if (!pathSeparatorStr) {
    return false;
  }

  JSRuntime* rt = cx->runtime();
  JS::SetModuleResolveHook(rt, ModuleLoader::ResolveImportedModule);
  JS::SetModuleMetadataHook(rt, ModuleLoader::GetImportMetaProperties);
  JS::SetModuleDynamicImportHook(rt, ModuleLoader::ImportModuleDynamically);

  return true;
}

// static
JSObject* ModuleLoader::ResolveImportedModule(
    JSContext* cx, JS::HandleValue referencingPrivate,
    JS::HandleObject moduleRequest) {
  ShellContext* scx = GetShellContext(cx);
  return scx->moduleLoader->resolveImportedModule(cx, referencingPrivate,
                                                  moduleRequest);
}

// static
bool ModuleLoader::GetImportMetaProperties(JSContext* cx,
                                           JS::HandleValue privateValue,
                                           JS::HandleObject metaObject) {
  ShellContext* scx = GetShellContext(cx);
  return scx->moduleLoader->populateImportMeta(cx, privateValue, metaObject);
}

// static
bool ModuleLoader::ImportModuleDynamically(JSContext* cx,
                                           JS::HandleValue referencingPrivate,
                                           JS::HandleObject moduleRequest,
                                           JS::HandleObject promise) {
  ShellContext* scx = GetShellContext(cx);
  return scx->moduleLoader->dynamicImport(cx, referencingPrivate, moduleRequest,
                                          promise);
}

bool ModuleLoader::loadRootModule(JSContext* cx, HandleString path) {
  RootedValue rval(cx);
  if (!loadAndExecute(cx, path, &rval)) {
    return false;
  }

  RootedObject evaluationPromise(cx, &rval.toObject());
  if (evaluationPromise == nullptr) {
    return false;
  }

  return JS::ThrowOnModuleEvaluationFailure(cx, evaluationPromise);
}

bool ModuleLoader::registerTestModule(JSContext* cx, HandleObject moduleRequest,
                                      HandleModuleObject module) {
  RootedLinearString path(cx, resolve(cx, moduleRequest, UndefinedHandleValue));
  if (!path) {
    return false;
  }

  path = normalizePath(cx, path);
  if (!path) {
    return false;
  }

  return addModuleToRegistry(cx, path, module);
}

bool ModuleLoader::loadAndExecute(JSContext* cx, HandleString path,
                                  MutableHandleValue rval) {
  RootedObject module(cx, loadAndParse(cx, path));
  if (!module) {
    return false;
  }

  if (!JS::ModuleInstantiate(cx, module)) {
    return false;
  }

  return JS::ModuleEvaluate(cx, module, rval);
}

JSObject* ModuleLoader::resolveImportedModule(
    JSContext* cx, JS::HandleValue referencingPrivate,
    JS::HandleObject moduleRequest) {
  RootedLinearString path(cx, resolve(cx, moduleRequest, referencingPrivate));
  if (!path) {
    return nullptr;
  }

  return loadAndParse(cx, path);
}

bool ModuleLoader::populateImportMeta(JSContext* cx,
                                      JS::HandleValue privateValue,
                                      JS::HandleObject metaObject) {
  RootedLinearString path(cx);
  if (!privateValue.isUndefined()) {
    if (!getScriptPath(cx, privateValue, &path)) {
      return false;
    }
  }

  if (!path) {
    path = NewStringCopyZ<CanGC>(cx, "(unknown)");
    if (!path) {
      return false;
    }
  }

  RootedValue pathValue(cx, StringValue(path));
  return JS_DefineProperty(cx, metaObject, "url", pathValue, JSPROP_ENUMERATE);
}

bool ModuleLoader::dynamicImport(JSContext* cx,
                                 JS::HandleValue referencingPrivate,
                                 JS::HandleObject moduleRequest,
                                 JS::HandleObject promise) {
  // To make this more realistic, use a promise to delay the import and make it
  // happen asynchronously. This method packages up the arguments and creates a
  // resolved promise, which on fullfillment calls doDynamicImport with the
  // original arguments.

  MOZ_ASSERT(promise);
  RootedValue moduleRequestValue(cx, ObjectValue(*moduleRequest));
  RootedValue promiseValue(cx, ObjectValue(*promise));
  RootedObject closure(cx, JS_NewObjectWithGivenProto(cx, nullptr, nullptr));
  if (!closure ||
      !JS_DefineProperty(cx, closure, "referencingPrivate", referencingPrivate,
                         JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, closure, "moduleRequest", moduleRequestValue,
                         JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, closure, "promise", promiseValue,
                         JSPROP_ENUMERATE)) {
    return false;
  }

  RootedFunction onResolved(
      cx, NewNativeFunction(cx, DynamicImportDelayFulfilled, 1, nullptr));
  if (!onResolved) {
    return false;
  }

  RootedFunction onRejected(
      cx, NewNativeFunction(cx, DynamicImportDelayRejected, 1, nullptr));
  if (!onRejected) {
    return false;
  }

  RootedObject delayPromise(cx);
  RootedValue closureValue(cx, ObjectValue(*closure));
  delayPromise = PromiseObject::unforgeableResolve(cx, closureValue);
  if (!delayPromise) {
    return false;
  }

  return JS::AddPromiseReactions(cx, delayPromise, onResolved, onRejected);
}

bool ModuleLoader::DynamicImportDelayFulfilled(JSContext* cx, unsigned argc,
                                               Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedObject closure(cx, &args[0].toObject());

  RootedValue referencingPrivate(cx);
  RootedValue moduleRequestValue(cx);
  RootedValue promiseValue(cx);
  if (!JS_GetProperty(cx, closure, "referencingPrivate", &referencingPrivate) ||
      !JS_GetProperty(cx, closure, "moduleRequest", &moduleRequestValue) ||
      !JS_GetProperty(cx, closure, "promise", &promiseValue)) {
    return false;
  }

  RootedObject moduleRequest(cx, &moduleRequestValue.toObject());
  RootedObject promise(cx, &promiseValue.toObject());

  ShellContext* scx = GetShellContext(cx);
  return scx->moduleLoader->doDynamicImport(cx, referencingPrivate,
                                            moduleRequest, promise);
}

bool ModuleLoader::DynamicImportDelayRejected(JSContext* cx, unsigned argc,
                                              Value* vp) {
  MOZ_CRASH("This promise should never be rejected");
}

bool ModuleLoader::doDynamicImport(JSContext* cx,
                                   JS::HandleValue referencingPrivate,
                                   JS::HandleObject moduleRequest,
                                   JS::HandleObject promise) {
  // Exceptions during dynamic import are handled by calling
  // FinishDynamicModuleImport with a pending exception on the context.
  RootedValue rval(cx);
  bool ok =
      tryDynamicImport(cx, referencingPrivate, moduleRequest, promise, &rval);
  JSObject* evaluationObject = ok ? &rval.toObject() : nullptr;
  RootedObject evaluationPromise(cx, evaluationObject);
  return JS::FinishDynamicModuleImport(
      cx, evaluationPromise, referencingPrivate, moduleRequest, promise);
}

bool ModuleLoader::tryDynamicImport(JSContext* cx,
                                    JS::HandleValue referencingPrivate,
                                    JS::HandleObject moduleRequest,
                                    JS::HandleObject promise,
                                    JS::MutableHandleValue rval) {
  RootedLinearString path(cx, resolve(cx, moduleRequest, referencingPrivate));
  if (!path) {
    return false;
  }

  return loadAndExecute(cx, path, rval);
}

JSLinearString* ModuleLoader::resolve(JSContext* cx,
                                      HandleObject moduleRequestArg,
                                      HandleValue referencingInfo) {
  ModuleRequestObject* moduleRequest =
      &moduleRequestArg->as<ModuleRequestObject>();
  if (moduleRequest->specifier()->length() == 0) {
    JS_ReportErrorASCII(cx, "Invalid module specifier");
    return nullptr;
  }

  RootedLinearString name(
      cx, JS_EnsureLinearString(cx, moduleRequest->specifier()));
  if (!name) {
    return nullptr;
  }

  if (IsJavaScriptURL(name) || IsAbsolutePath(name)) {
    return name;
  }

  // Treat |name| as a relative path if it starts with either "./" or "../".
  bool isRelative =
      StringStartsWith(name, u"./") || StringStartsWith(name, u"../")
#ifdef XP_WIN
      || StringStartsWith(name, u".\\") || StringStartsWith(name, u"..\\")
#endif
      ;

  RootedString path(cx, loadPathStr);

  if (isRelative) {
    if (referencingInfo.isUndefined()) {
      JS_ReportErrorASCII(cx, "No referencing module for relative import");
      return nullptr;
    }

    RootedLinearString refPath(cx);
    if (!getScriptPath(cx, referencingInfo, &refPath)) {
      return nullptr;
    }

    if (!refPath) {
      JS_ReportErrorASCII(cx, "No path set for referencing module");
      return nullptr;
    }

    int32_t sepIndex = LastIndexOf(refPath, u'/');
#ifdef XP_WIN
    sepIndex = std::max(sepIndex, LastIndexOf(refPath, u'\\'));
#endif
    if (sepIndex >= 0) {
      path = SubString(cx, refPath, 0, sepIndex);
      if (!path) {
        return nullptr;
      }
    }
  }

  RootedString result(cx);
  RootedString pathSep(cx, pathSeparatorStr);
  result = JS_ConcatStrings(cx, path, pathSep);
  if (!result) {
    return nullptr;
  }

  result = JS_ConcatStrings(cx, result, name);
  if (!result) {
    return nullptr;
  }

  return JS_EnsureLinearString(cx, result);
}

JSObject* ModuleLoader::loadAndParse(JSContext* cx, HandleString pathArg) {
  RootedLinearString path(cx, JS_EnsureLinearString(cx, pathArg));
  if (!path) {
    return nullptr;
  }

  path = normalizePath(cx, path);
  if (!path) {
    return nullptr;
  }

  RootedObject module(cx);
  if (!lookupModuleInRegistry(cx, path, &module)) {
    return nullptr;
  }

  if (module) {
    return module;
  }

  UniqueChars filename = JS_EncodeStringToLatin1(cx, path);
  if (!filename) {
    return nullptr;
  }

  JS::CompileOptions options(cx);
  options.setFileAndLine(filename.get(), 1);

  RootedString source(cx, fetchSource(cx, path));
  if (!source) {
    return nullptr;
  }

  JS::AutoStableStringChars stableChars(cx);
  if (!stableChars.initTwoByte(cx, source)) {
    return nullptr;
  }

  const char16_t* chars = stableChars.twoByteRange().begin().get();
  JS::SourceText<char16_t> srcBuf;
  if (!srcBuf.init(cx, chars, source->length(),
                   JS::SourceOwnership::Borrowed)) {
    return nullptr;
  }

  module = JS::CompileModule(cx, options, srcBuf);
  if (!module) {
    return nullptr;
  }

  RootedObject info(cx, CreateScriptPrivate(cx, path));
  if (!info) {
    return nullptr;
  }

  JS::SetModulePrivate(module, ObjectValue(*info));

  if (!addModuleToRegistry(cx, path, module)) {
    return nullptr;
  }

  return module;
}

bool ModuleLoader::lookupModuleInRegistry(JSContext* cx, HandleString path,
                                          MutableHandleObject moduleOut) {
  moduleOut.set(nullptr);

  RootedObject registry(cx, getOrCreateModuleRegistry(cx));
  if (!registry) {
    return false;
  }

  RootedValue pathValue(cx, StringValue(path));
  RootedValue moduleValue(cx);
  if (!JS::MapGet(cx, registry, pathValue, &moduleValue)) {
    return false;
  }

  if (!moduleValue.isUndefined()) {
    moduleOut.set(&moduleValue.toObject());
  }

  return true;
}

bool ModuleLoader::addModuleToRegistry(JSContext* cx, HandleString path,
                                       HandleObject module) {
  RootedObject registry(cx, getOrCreateModuleRegistry(cx));
  if (!registry) {
    return false;
  }

  RootedValue pathValue(cx, StringValue(path));
  RootedValue moduleValue(cx, ObjectValue(*module));
  return JS::MapSet(cx, registry, pathValue, moduleValue);
}

JSObject* ModuleLoader::getOrCreateModuleRegistry(JSContext* cx) {
  Handle<GlobalObject*> global = cx->global();
  RootedValue value(cx, global->getReservedSlot(GlobalAppSlotModuleRegistry));
  if (!value.isUndefined()) {
    return &value.toObject();
  }

  JSObject* registry = JS::NewMapObject(cx);
  if (!registry) {
    return nullptr;
  }

  global->setReservedSlot(GlobalAppSlotModuleRegistry, ObjectValue(*registry));
  return registry;
}

bool ModuleLoader::getScriptPath(JSContext* cx, HandleValue privateValue,
                                 MutableHandle<JSLinearString*> pathOut) {
  pathOut.set(nullptr);

  RootedObject infoObj(cx, &privateValue.toObject());
  RootedValue pathValue(cx);
  if (!JS_GetProperty(cx, infoObj, "path", &pathValue)) {
    return false;
  }

  if (pathValue.isUndefined()) {
    return true;
  }

  RootedString path(cx, pathValue.toString());
  pathOut.set(JS_EnsureLinearString(cx, path));
  return pathOut;
}

JSLinearString* ModuleLoader::normalizePath(JSContext* cx,
                                            HandleLinearString pathArg) {
  RootedLinearString path(cx, pathArg);

  if (IsJavaScriptURL(path)) {
    return path;
  }

#ifdef XP_WIN
  // Replace all forward slashes with backward slashes.
  path = ReplaceCharGlobally(cx, path, u'/', PathSeparator);
  if (!path) {
    return nullptr;
  }

  // Remove the drive letter, if present.
  RootedLinearString drive(cx);
  if (path->length() > 2 && mozilla::IsAsciiAlpha(CharAt(path, 0)) &&
      CharAt(path, 1) == u':' && CharAt(path, 2) == u'\\') {
    drive = SubString(cx, path, 0, 2);
    path = SubString(cx, path, 2);
    if (!drive || !path) {
      return nullptr;
    }
  }
#endif  // XP_WIN

  // Normalize the path by removing redundant path components.
  Rooted<GCVector<JSLinearString*>> components(cx);
  size_t lastSep = 0;
  while (lastSep < path->length()) {
    int32_t i = IndexOf(path, PathSeparator, lastSep);
    if (i < 0) {
      i = path->length();
    }

    RootedLinearString part(cx, SubString(cx, path, lastSep, i));
    if (!part) {
      return nullptr;
    }

    lastSep = i + 1;

    // Remove "." when preceded by a path component.
    if (StringEquals(part, u".") && !components.empty()) {
      continue;
    }

    if (StringEquals(part, u"..") && !components.empty()) {
      // Replace "./.." with "..".
      if (StringEquals(components.back(), u".")) {
        components.back() = part;
        continue;
      }

      // When preceded by a non-empty path component, remove ".." and the
      // preceding component, unless the preceding component is also "..".
      if (!StringEquals(components.back(), u"") &&
          !StringEquals(components.back(), u"..")) {
        components.popBack();
        continue;
      }
    }

    if (!components.append(part)) {
      return nullptr;
    }
  }

  RootedLinearString pathSep(cx, pathSeparatorStr);
  RootedString normalized(cx, JoinStrings(cx, components, pathSep));
  if (!normalized) {
    return nullptr;
  }

#ifdef XP_WIN
  if (drive) {
    normalized = JS_ConcatStrings(cx, drive, normalized);
    if (!normalized) {
      return nullptr;
    }
  }
#endif

  return JS_EnsureLinearString(cx, normalized);
}

JSString* ModuleLoader::fetchSource(JSContext* cx, HandleLinearString path) {
  if (IsJavaScriptURL(path)) {
    return ExtractJavaScriptURLSource(cx, path);
  }

  RootedString resolvedPath(cx, ResolvePath(cx, path, RootRelative));
  if (!resolvedPath) {
    return nullptr;
  }

  return FileAsString(cx, resolvedPath);
}
