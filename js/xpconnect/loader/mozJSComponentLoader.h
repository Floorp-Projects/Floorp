/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozJSComponentLoader_h
#define mozJSComponentLoader_h

#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/ModuleLoader.h"
#include "nsAutoPtr.h"
#include "nsISupports.h"
#include "nsIObserver.h"
#include "nsIURI.h"
#include "xpcIJSModuleLoader.h"
#include "nsClassHashtable.h"
#include "nsDataHashtable.h"
#include "jsapi.h"

#include "xpcIJSGetFactory.h"
#include "xpcpublic.h"

class nsIFile;
class ComponentLoaderInfo;

namespace mozilla {
    class ScriptPreloader;
} // namespace mozilla


/* 6bd13476-1dd2-11b2-bbef-f0ccb5fa64b6 (thanks, mozbot) */

#define MOZJSCOMPONENTLOADER_CID                                              \
  {0x6bd13476, 0x1dd2, 0x11b2,                                                \
    { 0xbb, 0xef, 0xf0, 0xcc, 0xb5, 0xfa, 0x64, 0xb6 }}
#define MOZJSCOMPONENTLOADER_CONTRACTID "@mozilla.org/moz/jsloader;1"

#if defined(NIGHTLY_BUILD) || defined(MOZ_DEV_EDITION) || defined(DEBUG)
#define STARTUP_RECORDER_ENABLED
#endif

class mozJSComponentLoader final : public mozilla::ModuleLoader,
                                   public xpcIJSModuleLoader,
                                   public nsIObserver
{
 public:
    NS_DECL_ISUPPORTS
    NS_DECL_XPCIJSMODULELOADER
    NS_DECL_NSIOBSERVER

    mozJSComponentLoader();

    // ModuleLoader
    const mozilla::Module* LoadModule(mozilla::FileLocation& aFile) override;

    void FindTargetObject(JSContext* aCx,
                          JS::MutableHandleObject aTargetObject);

    static already_AddRefed<mozJSComponentLoader> GetOrCreate();

    static mozJSComponentLoader* Get() { return sSelf; }

    nsresult ImportInto(const nsACString& aResourceURI, JS::HandleValue aTargetObj,
                        JSContext* aCx, uint8_t aArgc, JS::MutableHandleValue aRetval);

    nsresult Import(JSContext* aCx, const nsACString& aResourceURI,
                    JS::MutableHandleObject aModuleGlobal,
                    JS::MutableHandleObject aModuleExports,
                    bool aIgnoreExports = false);

    nsresult Unload(const nsACString& aResourceURI);
    nsresult IsModuleLoaded(const nsACString& aResourceURI, bool* aRetval);
    bool IsLoaderGlobal(JSObject* aObj) {
        return mLoaderGlobal == aObj;
    }

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf);

    /**
     * Temporary diagnostic function for startup crashes in bug 1403348:
     *
     * Annotate the crash report with the contents of the async shutdown
     * module/component scripts.
     */
    nsresult AnnotateCrashReport();

 protected:
    virtual ~mozJSComponentLoader();

    friend class XPCJSRuntime;

    JSObject* CompilationScope(JSContext* aCx)
    {
        if (mLoaderGlobal)
            return mLoaderGlobal;
        return GetSharedGlobal(aCx);
    }

 private:
    static mozJSComponentLoader* sSelf;

    nsresult ReallyInit();
    void UnloadModules();

    void CreateLoaderGlobal(JSContext* aCx,
                            const nsACString& aLocation,
                            JS::MutableHandleObject aGlobal);

    bool ReuseGlobal(nsIURI* aComponent);

    JSObject* GetSharedGlobal(JSContext* aCx);

    JSObject* PrepareObjectForLocation(JSContext* aCx,
                                       nsIFile* aComponentFile,
                                       nsIURI* aComponent,
                                       bool* aReuseGlobal,
                                       bool* aRealFile);

    nsresult ObjectForLocation(ComponentLoaderInfo& aInfo,
                               nsIFile* aComponentFile,
                               JS::MutableHandleObject aObject,
                               JS::MutableHandleScript aTableScript,
                               char** location,
                               bool aCatchException,
                               JS::MutableHandleValue aException);

    nsresult ImportInto(const nsACString& aLocation,
                        JS::HandleObject targetObj,
                        JSContext* callercx,
                        JS::MutableHandleObject vp);

    nsCOMPtr<nsIComponentManager> mCompMgr;

    class ModuleEntry : public mozilla::Module
    {
    public:
        explicit ModuleEntry(JS::RootingContext* aRootingCx)
          : mozilla::Module(), obj(aRootingCx), exports(aRootingCx),
            thisObjectKey(aRootingCx)
        {
            mVersion = mozilla::Module::kVersion;
            mCIDs = nullptr;
            mContractIDs = nullptr;
            mCategoryEntries = nullptr;
            getFactoryProc = GetFactory;
            loadProc = nullptr;
            unloadProc = nullptr;

            location = nullptr;
        }

        ~ModuleEntry() {
            Clear();
        }

        void Clear() {
            getfactoryobj = nullptr;

            if (obj) {
                mozilla::AutoJSContext cx;
                JSAutoRealm ar(cx, obj);

                if (JS_HasExtensibleLexicalEnvironment(obj)) {
                    JS_SetAllNonReservedSlotsToUndefined(cx, JS_ExtensibleLexicalEnvironment(obj));
                }
                JS_SetAllNonReservedSlotsToUndefined(cx, obj);
                obj = nullptr;
                thisObjectKey = nullptr;
            }

            if (location)
                free(location);

            obj = nullptr;
            thisObjectKey = nullptr;
            location = nullptr;
#ifdef STARTUP_RECORDER_ENABLED
            importStack.Truncate();
#endif
        }

        size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

        static already_AddRefed<nsIFactory> GetFactory(const mozilla::Module& module,
                                                       const mozilla::Module::CIDEntry& entry);

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
                            ModuleEntry* aMod,
                            JS::MutableHandleObject aExports);

    static size_t DataEntrySizeOfExcludingThis(const nsACString& aKey, ModuleEntry* const& aData,
                                               mozilla::MallocSizeOf aMallocSizeOf, void* arg);
    static size_t ClassEntrySizeOfExcludingThis(const nsACString& aKey,
                                                const nsAutoPtr<ModuleEntry>& aData,
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
