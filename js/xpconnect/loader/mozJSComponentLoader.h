/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozJSComponentLoader_h
#define mozJSComponentLoader_h

#include "ComponentModuleLoader.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/FileLocation.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Module.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsIMemoryReporter.h"
#include "nsISupports.h"
#include "nsIURI.h"
#include "nsClassHashtable.h"
#include "nsTHashMap.h"
#include "jsapi.h"
#include "js/experimental/JSStencil.h"

#include "xpcIJSGetFactory.h"
#include "xpcpublic.h"

class nsIFile;
class ComponentLoaderInfo;

namespace mozilla {
class ScriptPreloader;
}  // namespace mozilla

#if defined(NIGHTLY_BUILD) || defined(MOZ_DEV_EDITION) || defined(DEBUG)
#  define STARTUP_RECORDER_ENABLED
#endif

class mozJSComponentLoader final : public nsIMemoryReporter {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMEMORYREPORTER

  // Returns the list of all JSMs.
  void GetLoadedModules(nsTArray<nsCString>& aLoadedModules);

  // Returns the list of all ESMs.
  nsresult GetLoadedESModules(nsTArray<nsCString>& aLoadedModules);

  // Returns the list of all JSMs and ESMs.
  nsresult GetLoadedJSAndESModules(nsTArray<nsCString>& aLoadedModules);

  void GetLoadedComponents(nsTArray<nsCString>& aLoadedComponents);
  nsresult GetModuleImportStack(const nsACString& aLocation,
                                nsACString& aRetval);
  nsresult GetComponentLoadStack(const nsACString& aLocation,
                                 nsACString& aRetval);

  const mozilla::Module* LoadModule(mozilla::FileLocation& aFile);

  void FindTargetObject(JSContext* aCx, JS::MutableHandleObject aTargetObject);

  static void InitStatics();
  static void Unload();
  static void Shutdown();

  static mozJSComponentLoader* Get() {
    MOZ_ASSERT(sSelf, "Should have already created the component loader");
    return sSelf;
  }

  nsresult ImportInto(const nsACString& aResourceURI,
                      JS::HandleValue aTargetObj, JSContext* aCx, uint8_t aArgc,
                      JS::MutableHandleValue aRetval);

  // Load a JSM.
  nsresult Import(JSContext* aCx, const nsACString& aResourceURI,
                  JS::MutableHandleObject aModuleGlobal,
                  JS::MutableHandleObject aModuleExports,
                  bool aIgnoreExports = false);

  // Load an ES6 module and all its dependencies.
  nsresult ImportESModule(JSContext* aCx, const nsACString& aResourceURI,
                          JS::MutableHandleObject aModuleNamespace);

  // Fallback from Import to ImportESModule.
  nsresult TryFallbackToImportESModule(JSContext* aCx,
                                       const nsACString& aResourceURI,
                                       JS::MutableHandleObject aModuleGlobal,
                                       JS::MutableHandleObject aModuleExports,
                                       bool aIgnoreExports);

  nsresult Unload(const nsACString& aResourceURI);
  nsresult IsModuleLoaded(const nsACString& aResourceURI, bool* aRetval);
  bool IsLoaderGlobal(JSObject* aObj) { return mLoaderGlobal == aObj; }

  // Public methods for use from ComponentModuleLoader.
  static bool IsTrustedScheme(nsIURI* aURI);
  static nsresult LoadSingleModuleScript(JSContext* aCx, nsIURI* aURI,
                                         JS::MutableHandleScript aScriptOut);

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf);

 protected:
  mozJSComponentLoader();
  ~mozJSComponentLoader();

  friend class XPCJSRuntime;

 private:
  static mozilla::StaticRefPtr<mozJSComponentLoader> sSelf;

  void UnloadModules();

  void CreateLoaderGlobal(JSContext* aCx, const nsACString& aLocation,
                          JS::MutableHandleObject aGlobal);

  JSObject* GetSharedGlobal(JSContext* aCx);

  static nsresult GetSourceFile(nsIURI* aResolvedURI, nsIFile** aSourceFileOut);

  static bool LocationIsRealFile(nsIURI* aURI);

  JSObject* PrepareObjectForLocation(JSContext* aCx, nsIFile* aComponentFile,
                                     nsIURI* aComponent, bool aRealFile);

  nsresult ObjectForLocation(ComponentLoaderInfo& aInfo,
                             nsIFile* aComponentFile,
                             JS::MutableHandleObject aObject,
                             JS::MutableHandleScript aTableScript,
                             char** aLocation, bool aCatchException,
                             JS::MutableHandleValue aException);

  // Get the script for a given location, either from a cached stencil or by
  // compiling it from source.
  static nsresult GetScriptForLocation(JSContext* aCx,
                                       ComponentLoaderInfo& aInfo,
                                       nsIFile* aComponentFile, bool aUseMemMap,
                                       JS::MutableHandleScript aScriptOut,
                                       char** aLocationOut = nullptr);

  static already_AddRefed<JS::Stencil> CompileStencil(
      JSContext* aCx, const JS::CompileOptions& aOptions,
      JS::SourceText<mozilla::Utf8Unit>& aSource, bool aIsModule);
  static JSScript* InstantiateStencil(JSContext* aCx, JS::Stencil* aStencil,
                                      bool aIsModule);

  nsresult ImportInto(const nsACString& aLocation, JS::HandleObject targetObj,
                      JSContext* callercx, JS::MutableHandleObject vp);

  nsCOMPtr<nsIComponentManager> mCompMgr;

  class ModuleEntry : public mozilla::Module {
   public:
    explicit ModuleEntry(JS::RootingContext* aRootingCx)
        : mozilla::Module(),
          obj(aRootingCx),
          exports(aRootingCx),
          thisObjectKey(aRootingCx) {
      mVersion = mozilla::Module::kVersion;
      mCIDs = nullptr;
      mContractIDs = nullptr;
      mCategoryEntries = nullptr;
      getFactoryProc = GetFactory;
      loadProc = nullptr;
      unloadProc = nullptr;

      location = nullptr;
    }

    ~ModuleEntry() { Clear(); }

    void Clear() {
      getfactoryobj = nullptr;

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
#ifdef STARTUP_RECORDER_ENABLED
      importStack.Truncate();
#endif
    }

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

    static already_AddRefed<nsIFactory> GetFactory(
        const mozilla::Module& module, const mozilla::Module::CIDEntry& entry);

    nsCOMPtr<xpcIJSGetFactory> getfactoryobj;
    JS::PersistentRootedObject obj;
    JS::PersistentRootedObject exports;
    JS::PersistentRootedScript thisObjectKey;
    char* location;
    nsCString resolvedURL;
#ifdef STARTUP_RECORDER_ENABLED
    nsCString importStack;
#endif
  };

  nsresult ExtractExports(JSContext* aCx, ComponentLoaderInfo& aInfo,
                          ModuleEntry* aMod, JS::MutableHandleObject aExports);

  // Modules are intentionally leaked, but still cleared.
  nsTHashMap<nsCStringHashKey, ModuleEntry*> mModules;

  nsClassHashtable<nsCStringHashKey, ModuleEntry> mImports;
  nsTHashMap<nsCStringHashKey, ModuleEntry*> mInProgressImports;

  // A map of on-disk file locations which are loaded as modules to the
  // pre-resolved URIs they were loaded from. Used to prevent the same file
  // from being loaded separately, from multiple URLs.
  nsClassHashtable<nsCStringHashKey, nsCString> mLocations;

  bool mInitialized;
  JS::PersistentRooted<JSObject*> mLoaderGlobal;

  RefPtr<mozilla::loader::ComponentModuleLoader> mModuleLoader;
};

#endif
