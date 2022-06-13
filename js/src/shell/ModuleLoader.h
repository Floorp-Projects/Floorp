/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef shell_ModuleLoader_h
#define shell_ModuleLoader_h

#include "builtin/ModuleObject.h"
#include "gc/Rooting.h"
#include "js/RootingAPI.h"

namespace js {
namespace shell {

class ModuleLoader {
 public:
  bool init(JSContext* cx, HandleString loadPath);
  bool loadRootModule(JSContext* cx, HandleString path);

  // Testing hook to register a module that wasn't loaded by the module loader.
  bool registerTestModule(JSContext* cx, HandleObject moduleRequest,
                          Handle<ModuleObject*> module);

 private:
  static JSObject* ResolveImportedModule(JSContext* cx,
                                         HandleValue referencingPrivate,
                                         HandleObject moduleRequest);
  static bool GetImportMetaProperties(JSContext* cx, HandleValue privateValue,
                                      HandleObject metaObject);
  static bool ImportModuleDynamically(JSContext* cx,
                                      HandleValue referencingPrivate,
                                      HandleObject moduleRequest,
                                      HandleObject promise);
  static bool GetSupportedImportAssertions(JSContext* cx,
                                           JS::ImportAssertionVector& values);

  static bool DynamicImportDelayFulfilled(JSContext* cx, unsigned argc,
                                          Value* vp);
  static bool DynamicImportDelayRejected(JSContext* cx, unsigned argc,
                                         Value* vp);

  bool loadAndExecute(JSContext* cx, HandleString path, MutableHandleValue);
  JSObject* resolveImportedModule(JSContext* cx, HandleValue referencingPrivate,
                                  HandleObject moduleRequest);
  bool populateImportMeta(JSContext* cx, HandleValue privateValue,
                          HandleObject metaObject);
  bool dynamicImport(JSContext* cx, HandleValue referencingPrivate,
                     HandleObject moduleRequest, HandleObject promise);
  bool doDynamicImport(JSContext* cx, HandleValue referencingPrivate,
                       HandleObject moduleRequest, HandleObject promise);
  bool tryDynamicImport(JSContext* cx, HandleValue referencingPrivate,
                        HandleObject moduleRequest, HandleObject promise,
                        MutableHandleValue rval);
  JSObject* loadAndParse(JSContext* cx, HandleString path);
  bool lookupModuleInRegistry(JSContext* cx, HandleString path,
                              MutableHandleObject moduleOut);
  bool addModuleToRegistry(JSContext* cx, HandleString path,
                           HandleObject module);
  JSLinearString* resolve(JSContext* cx, HandleObject moduleRequestArg,
                          HandleValue referencingInfo);
  bool getScriptPath(JSContext* cx, HandleValue privateValue,
                     MutableHandle<JSLinearString*> pathOut);
  JSLinearString* normalizePath(JSContext* cx, Handle<JSLinearString*> path);
  JSObject* getOrCreateModuleRegistry(JSContext* cx);
  JSString* fetchSource(JSContext* cx, Handle<JSLinearString*> path);

  // The following are used for pinned atoms which do not need rooting.
  JSAtom* loadPathStr = nullptr;
  JSAtom* pathSeparatorStr = nullptr;
} JS_HAZ_NON_GC_POINTER;

}  // namespace shell
}  // namespace js

#endif  // shell_ModuleLoader_h
