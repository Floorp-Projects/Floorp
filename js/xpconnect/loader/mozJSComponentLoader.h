/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozJSComponentLoader_h
#define mozJSComponentLoader_h

#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/FileLocation.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Module.h"
#include "mozilla/StaticPtr.h"
#include "nsAutoPtr.h"
#include "nsISupports.h"
#include "nsIURI.h"
#include "nsClassHashtable.h"
#include "nsDataHashtable.h"
#include "jsapi.h"

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

class mozJSComponentLoader final {
 public:
  NS_INLINE_DECL_REFCOUNTING(mozJSComponentLoader);

  void GetLoadedModules(nsTArray<nsCString>& aLoadedModules);
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

  nsresult Import(JSContext* aCx, const nsACString& aResourceURI,
                  JS::MutableHandleObject aModuleGlobal,
                  JS::MutableHandleObject aModuleExports,
                  bool aIgnoreExports = false);

  nsresult Unload(const nsACString& aResourceURI);
  nsresult IsModuleLoaded(const nsACString& aResourceURI, bool* aRetval);
  bool IsLoaderGlobal(JSObject* aObj) { return mLoaderGlobal == aObj; }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf);

  /**
   * Temporary diagnostic function for startup crashes in bug 1403348:
   *
   * Annotate the crash report with the contents of the async shutdown
   * module/component scripts.
   */
  nsresult AnnotateCrashReport();

 protected:
  mozJSComponentLoader();
  ~mozJSComponentLoader();

  friend class XPCJSRuntime;

  JSObject* CompilationScope(JSContext* aCx) {
    if (mLoaderGlobal) {
      return mLoaderGlobal;
    }
    return GetSharedGlobal(aCx);
  }

 private:
  static mozilla::StaticRefPtr<mozJSComponentLoader> sSelf;

  nsresult ReallyInit();
  void UnloadModules();

  void CreateLoaderGlobal(JSContext* aCx, const nsACString& aLocation,
                          JS::MutableHandleObject aGlobal);

  bool ReuseGlobal(nsIURI* aComponent);

  JSObject* GetSharedGlobal(JSContext* aCx);

  JSObject* PrepareObjectForLocation(JSContext* aCx, nsIFile* aComponentFile,
                                     nsIURI* aComponent, bool* aReuseGlobal,
                                     bool* aRealFile);

  nsresult ObjectForLocation(ComponentLoaderInfo& aInfo,
                             nsIFile* aComponentFile,
                             JS::MutableHandleObject aObject,
                             JS::MutableHandleScript aTableScript,
                             char** location, bool aCatchException,
                             JS::MutableHandleValue aException);

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

  static size_t DataEntrySizeOfExcludingThis(
      const nsACString& aKey, ModuleEntry* const& aData,
      mozilla::MallocSizeOf aMallocSizeOf, void* arg);
  static size_t ClassEntrySizeOfExcludingThis(
      const nsACString& aKey, const nsAutoPtr<ModuleEntry>& aData,
      mozilla::MallocSizeOf aMallocSizeOf, void* arg);

  // Modules are intentionally leaked, but still cleared.
  nsDataHashtable<nsCStringHashKey, ModuleEntry*> mModules;

  nsClassHashtable<nsCStringHashKey, ModuleEntry> mImports;
  nsDataHashtable<nsCStringHashKey, ModuleEntry*> mInProgressImports;

  // A map of on-disk file locations which are loaded as modules to the
  // pre-resolved URIs they were loaded from. Used to prevent the same file
  // from being loaded separately, from multiple URLs.
  nsClassHashtable<nsCStringHashKey, nsCString> mLocations;

  bool mInitialized;
  bool mShareLoaderGlobal;
  JS::PersistentRooted<JSObject*> mLoaderGlobal;
};

#endif
