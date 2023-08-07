/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozJSModuleLoader_h
#define mozJSModuleLoader_h

#include "ComponentModuleLoader.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/FileLocation.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/StaticPtr.h"
#include "nsIMemoryReporter.h"
#include "nsISupports.h"
#include "nsIURI.h"
#include "nsClassHashtable.h"
#include "jsapi.h"
#include "js/experimental/JSStencil.h"
#include "SkipCheckForBrokenURLOrZeroSized.h"

#include "xpcpublic.h"

class nsIFile;
class ModuleLoaderInfo;

namespace mozilla {
class ScriptPreloader;
}  // namespace mozilla

namespace JS::loader {
class ModuleLoadRequest;
}  // namespace JS::loader

#if defined(NIGHTLY_BUILD) || defined(MOZ_DEV_EDITION) || defined(DEBUG)
#  define STARTUP_RECORDER_ENABLED
#endif

class mozJSModuleLoader final : public nsIMemoryReporter {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMEMORYREPORTER

  // Returns the list of all JSMs.
  void GetLoadedModules(nsTArray<nsCString>& aLoadedModules);

  // Returns the list of all ESMs.
  nsresult GetLoadedESModules(nsTArray<nsCString>& aLoadedModules);

  // Returns the list of all JSMs and ESMs.
  nsresult GetLoadedJSAndESModules(nsTArray<nsCString>& aLoadedModules);

  nsresult GetModuleImportStack(const nsACString& aLocation,
                                nsACString& aRetval);

  void FindTargetObject(JSContext* aCx, JS::MutableHandleObject aTargetObject);

  static void InitStatics();
  static void UnloadLoaders();
  static void ShutdownLoaders();

  static mozJSModuleLoader* Get() {
    MOZ_ASSERT(sSelf, "Should have already created the module loader");
    return sSelf;
  }

  static mozJSModuleLoader* GetDevToolsLoader() { return sDevToolsLoader; }
  static mozJSModuleLoader* GetOrCreateDevToolsLoader();

  nsresult ImportInto(const nsACString& aResourceURI,
                      JS::HandleValue aTargetObj, JSContext* aCx, uint8_t aArgc,
                      JS::MutableHandleValue aRetval);

  // Load a JSM.
  nsresult Import(JSContext* aCx, const nsACString& aResourceURI,
                  JS::MutableHandleObject aModuleGlobal,
                  JS::MutableHandleObject aModuleExports,
                  bool aIgnoreExports = false);

  // Synchronously load an ES6 module and all its dependencies.
  nsresult ImportESModule(
      JSContext* aCx, const nsACString& aResourceURI,
      JS::MutableHandleObject aModuleNamespace,
      mozilla::loader::SkipCheckForBrokenURLOrZeroSized aSkipCheck =
          mozilla::loader::SkipCheckForBrokenURLOrZeroSized::No);

  // Fallback from Import to ImportESModule.
  nsresult TryFallbackToImportESModule(JSContext* aCx,
                                       const nsACString& aResourceURI,
                                       JS::MutableHandleObject aModuleGlobal,
                                       JS::MutableHandleObject aModuleExports,
                                       bool aIgnoreExports);

  // If the request was handled by fallback before, fills the output and
  // sets *aFound to true and returns NS_OK.
  // If the request wasn't yet handled by fallback, sets *Found to false
  // and returns NS_OK.
  nsresult TryCachedFallbackToImportESModule(
      JSContext* aCx, const nsACString& aResourceURI,
      JS::MutableHandleObject aModuleGlobal,
      JS::MutableHandleObject aModuleExports, bool aIgnoreExports,
      bool* aFound);

#ifdef STARTUP_RECORDER_ENABLED
  void RecordImportStack(JSContext* aCx, const nsACString& aLocation);
  void RecordImportStack(JSContext* aCx,
                         JS::loader::ModuleLoadRequest* aRequest);
#endif

  nsresult Unload(const nsACString& aResourceURI);
  nsresult IsModuleLoaded(const nsACString& aResourceURI, bool* aRetval);
  nsresult IsJSModuleLoaded(const nsACString& aResourceURI, bool* aRetval);
  nsresult IsESModuleLoaded(const nsACString& aResourceURI, bool* aRetval);
  bool IsLoaderGlobal(JSObject* aObj) { return mLoaderGlobal == aObj; }
  bool IsDevToolsLoader() const { return this == sDevToolsLoader; }

  // Public methods for use from ComponentModuleLoader.
  static bool IsTrustedScheme(nsIURI* aURI);
  static nsresult LoadSingleModuleScript(
      mozilla::loader::ComponentModuleLoader* aModuleLoader, JSContext* aCx,
      JS::loader::ModuleLoadRequest* aRequest,
      JS::MutableHandleScript aScriptOut);

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf);

  bool DefineJSServices(JSContext* aCx, JS::Handle<JSObject*> aGlobal);

 protected:
  mozJSModuleLoader();
  ~mozJSModuleLoader();

  friend class XPCJSRuntime;

 private:
  static mozilla::StaticRefPtr<mozJSModuleLoader> sSelf;
  static mozilla::StaticRefPtr<mozJSModuleLoader> sDevToolsLoader;

  void Unload();
  void UnloadModules();

  void CreateLoaderGlobal(JSContext* aCx, const nsACString& aLocation,
                          JS::MutableHandleObject aGlobal);
  void CreateDevToolsLoaderGlobal(JSContext* aCx, const nsACString& aLocation,
                                  JS::MutableHandleObject aGlobal);

  bool CreateJSServices(JSContext* aCx);

  JSObject* GetSharedGlobal(JSContext* aCx);

  static nsresult GetSourceFile(nsIURI* aResolvedURI, nsIFile** aSourceFileOut);

  static bool LocationIsRealFile(nsIURI* aURI);

  JSObject* PrepareObjectForLocation(JSContext* aCx, nsIFile* aModuleFile,
                                     nsIURI* aURI, bool aRealFile);

  nsresult ObjectForLocation(ModuleLoaderInfo& aInfo, nsIFile* aModuleFile,
                             JS::MutableHandleObject aObject,
                             JS::MutableHandleScript aTableScript,
                             char** aLocation, bool aCatchException,
                             JS::MutableHandleValue aException);

  // Get the script for a given location, either from a cached stencil or by
  // compiling it from source.
  static nsresult GetScriptForLocation(JSContext* aCx, ModuleLoaderInfo& aInfo,
                                       nsIFile* aModuleFile, bool aUseMemMap,
                                       JS::MutableHandleScript aScriptOut,
                                       char** aLocationOut = nullptr);

  static already_AddRefed<JS::Stencil> CompileStencil(
      JSContext* aCx, const JS::CompileOptions& aOptions,
      JS::SourceText<mozilla::Utf8Unit>& aSource, bool aIsModule);
  static JSScript* InstantiateStencil(JSContext* aCx, JS::Stencil* aStencil,
                                      bool aIsModule);

  nsresult ImportInto(const nsACString& aLocation, JS::HandleObject targetObj,
                      JSContext* callercx, JS::MutableHandleObject vp);

  class ModuleEntry {
   public:
    explicit ModuleEntry(JS::RootingContext* aRootingCx)
        : obj(aRootingCx), exports(aRootingCx), thisObjectKey(aRootingCx) {
      location = nullptr;
    }

    ~ModuleEntry() { Clear(); }

    void Clear() {
      if (obj) {
        if (JS_HasExtensibleLexicalEnvironment(obj)) {
          JS::RootedObject lexicalEnv(mozilla::dom::RootingCx(),
                                      JS_ExtensibleLexicalEnvironment(obj));
          JS_SetAllNonReservedSlotsToUndefined(lexicalEnv);
        }
        JS_SetAllNonReservedSlotsToUndefined(obj);
        obj = nullptr;
        thisObjectKey = nullptr;
      }

      if (location) {
        free(location);
      }

      obj = nullptr;
      thisObjectKey = nullptr;
      location = nullptr;
    }

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

    JS::PersistentRootedObject obj;
    JS::PersistentRootedObject exports;
    JS::PersistentRootedScript thisObjectKey;
    char* location;
    nsCString resolvedURL;
  };

  class FallbackModuleEntry {
   public:
    explicit FallbackModuleEntry(JS::RootingContext* aRootingCx)
        : globalProxy(aRootingCx), moduleNamespace(aRootingCx) {}

    ~FallbackModuleEntry() { Clear(); }

    void Clear() {
      globalProxy = nullptr;
      moduleNamespace = nullptr;
    }

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
      return aMallocSizeOf(this);
    }

    JS::PersistentRootedObject globalProxy;
    JS::PersistentRootedObject moduleNamespace;
  };

  nsresult ExtractExports(JSContext* aCx, ModuleLoaderInfo& aInfo,
                          ModuleEntry* aMod, JS::MutableHandleObject aExports);

  nsClassHashtable<nsCStringHashKey, ModuleEntry> mImports;
  nsTHashMap<nsCStringHashKey, ModuleEntry*> mInProgressImports;
  nsClassHashtable<nsCStringHashKey, FallbackModuleEntry> mFallbackImports;
#ifdef STARTUP_RECORDER_ENABLED
  nsTHashMap<nsCStringHashKey, nsCString> mImportStacks;
#endif

  // A map of on-disk file locations which are loaded as modules to the
  // pre-resolved URIs they were loaded from. Used to prevent the same file
  // from being loaded separately, from multiple URLs.
  nsClassHashtable<nsCStringHashKey, nsCString> mLocations;

  bool mInitialized;
#ifdef DEBUG
  bool mIsInitializingLoaderGlobal = false;
#endif
  JS::PersistentRooted<JSObject*> mLoaderGlobal;
  JS::PersistentRooted<JSObject*> mServicesObj;

  RefPtr<mozilla::loader::ComponentModuleLoader> mModuleLoader;
};

#endif
