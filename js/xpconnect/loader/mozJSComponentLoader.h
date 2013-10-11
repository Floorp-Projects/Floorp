/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleLoader.h"
#include "nsISupports.h"
#include "nsIObserver.h"
#include "xpcIJSModuleLoader.h"
#include "nsClassHashtable.h"
#include "nsDataHashtable.h"
#include "jsapi.h"

#include "xpcIJSGetFactory.h"

class nsIFile;
class nsIJSRuntimeService;
class nsIPrincipal;
class nsIXPConnectJSObjectHolder;

/* 6bd13476-1dd2-11b2-bbef-f0ccb5fa64b6 (thanks, mozbot) */

#define MOZJSCOMPONENTLOADER_CID                                              \
  {0x6bd13476, 0x1dd2, 0x11b2,                                                \
    { 0xbb, 0xef, 0xf0, 0xcc, 0xb5, 0xfa, 0x64, 0xb6 }}
#define MOZJSCOMPONENTLOADER_CONTRACTID "@mozilla.org/moz/jsloader;1"

class JSCLContextHelper;

class mozJSComponentLoader : public mozilla::ModuleLoader,
                             public xpcIJSModuleLoader,
                             public nsIObserver
{
    friend class JSCLContextHelper;
 public:
    NS_DECL_ISUPPORTS
    NS_DECL_XPCIJSMODULELOADER
    NS_DECL_NSIOBSERVER

    mozJSComponentLoader();
    virtual ~mozJSComponentLoader();

    // ModuleLoader
    const mozilla::Module* LoadModule(mozilla::FileLocation &aFile);

    nsresult FindTargetObject(JSContext* aCx,
                              JS::MutableHandleObject aTargetObject);

    static mozJSComponentLoader* Get() { return sSelf; }

    void NoteSubScript(JS::HandleScript aScript, JS::HandleObject aThisObject);

 protected:
    static mozJSComponentLoader* sSelf;

    nsresult ReallyInit();
    void UnloadModules();

    JSObject* PrepareObjectForLocation(JSCLContextHelper& aCx,
                                       nsIFile* aComponentFile,
                                       nsIURI *aComponent,
                                       bool aReuseLoaderGlobal,
                                       bool *aRealFile);

    nsresult ObjectForLocation(nsIFile* aComponentFile,
                               nsIURI *aComponent,
                               JSObject **aObject,
                               char **location,
                               bool aCatchException,
                               JS::MutableHandleValue aException);

    nsresult ImportInto(const nsACString &aLocation,
                        JS::HandleObject targetObj,
                        JSContext *callercx,
                        JS::MutableHandleObject vp);

    nsCOMPtr<nsIComponentManager> mCompMgr;
    nsCOMPtr<nsIJSRuntimeService> mRuntimeService;
    nsCOMPtr<nsIPrincipal> mSystemPrincipal;
    nsCOMPtr<nsIXPConnectJSObjectHolder> mLoaderGlobal;
    JSRuntime *mRuntime;
    JSContext *mContext;

    class ModuleEntry : public mozilla::Module
    {
    public:
        ModuleEntry() : mozilla::Module() {
            mVersion = mozilla::Module::kVersion;
            mCIDs = nullptr;
            mContractIDs = nullptr;
            mCategoryEntries = nullptr;
            getFactoryProc = GetFactory;
            loadProc = nullptr;
            unloadProc = nullptr;

            obj = nullptr;
            location = nullptr;
        }

        ~ModuleEntry() {
            Clear();
        }

        void Clear() {
            getfactoryobj = nullptr;

            if (obj) {
                JSAutoRequest ar(sSelf->mContext);

                JSAutoCompartment ac(sSelf->mContext, obj);

                JS_SetAllNonReservedSlotsToUndefined(sSelf->mContext, obj);
                JS_RemoveObjectRoot(sSelf->mContext, &obj);
            }

            if (location)
                NS_Free(location);

            obj = nullptr;
            location = nullptr;
        }

        static already_AddRefed<nsIFactory> GetFactory(const mozilla::Module& module,
                                                       const mozilla::Module::CIDEntry& entry);

        nsCOMPtr<xpcIJSGetFactory> getfactoryobj;
        JSObject            *obj;
        char                *location;
    };

    friend class ModuleEntry;

    // Modules are intentionally leaked, but still cleared.
    static PLDHashOperator ClearModules(const nsACString& key, ModuleEntry*& entry, void* cx);
    nsDataHashtable<nsCStringHashKey, ModuleEntry*> mModules;

    nsClassHashtable<nsCStringHashKey, ModuleEntry> mImports;
    nsDataHashtable<nsCStringHashKey, ModuleEntry*> mInProgressImports;
    nsDataHashtable<nsPtrHashKey<JSScript>, JSObject*> mThisObjects;

    bool mInitialized;
    bool mReuseLoaderGlobal;
};
