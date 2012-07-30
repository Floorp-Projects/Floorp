/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "plhash.h"
#include "jsapi.h"
#include "mozilla/ModuleLoader.h"
#include "nsIJSRuntimeService.h"
#include "nsIJSContextStack.h"
#include "nsISupports.h"
#include "nsIXPConnect.h"
#include "nsIFile.h"
#include "nsAutoPtr.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsITimer.h"
#include "nsIObserver.h"
#include "xpcIJSModuleLoader.h"
#include "nsClassHashtable.h"
#include "nsDataHashtable.h"
#include "nsIPrincipal.h"
#include "mozilla/scache/StartupCache.h"

#include "xpcIJSGetFactory.h"

/* 6bd13476-1dd2-11b2-bbef-f0ccb5fa64b6 (thanks, mozbot) */

#define MOZJSCOMPONENTLOADER_CID                                              \
  {0x6bd13476, 0x1dd2, 0x11b2,                                                \
    { 0xbb, 0xef, 0xf0, 0xcc, 0xb5, 0xfa, 0x64, 0xb6 }}
#define MOZJSCOMPONENTLOADER_CONTRACTID "@mozilla.org/moz/jsloader;1"

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

 protected:
    static mozJSComponentLoader* sSelf;

    nsresult ReallyInit();
    void UnloadModules();

    nsresult GlobalForLocation(nsIFile* aComponentFile,
                               nsIURI *aComponent,
                               JSObject **aGlobal,
                               char **location,
                               jsval *exception);

    nsresult ImportInto(const nsACString & aLocation,
                        JSObject * targetObj,
                        JSContext * callercx,
                        JSObject * *_retval);

    nsCOMPtr<nsIComponentManager> mCompMgr;
    nsCOMPtr<nsIJSRuntimeService> mRuntimeService;
    nsCOMPtr<nsIThreadJSContextStack> mContextStack;
    nsCOMPtr<nsIPrincipal> mSystemPrincipal;
    JSRuntime *mRuntime;
    JSContext *mContext;

    class ModuleEntry : public mozilla::Module
    {
    public:
        ModuleEntry() : mozilla::Module() {
            mVersion = mozilla::Module::kVersion;
            mCIDs = NULL;
            mContractIDs = NULL;
            mCategoryEntries = NULL;
            getFactoryProc = GetFactory;
            loadProc = NULL;
            unloadProc = NULL;

            global = nullptr;
            location = nullptr;
        }

        ~ModuleEntry() {
            Clear();
        }

        void Clear() {
            getfactoryobj = NULL;

            if (global) {
                JSAutoRequest ar(sSelf->mContext);

                JSAutoEnterCompartment ac;
                ac.enterAndIgnoreErrors(sSelf->mContext, global);

                JS_ClearScope(sSelf->mContext, global);
                JS_RemoveObjectRoot(sSelf->mContext, &global);
            }

            if (location)
                NS_Free(location);

            global = NULL;
            location = NULL;
        }

        static already_AddRefed<nsIFactory> GetFactory(const mozilla::Module& module,
                                                       const mozilla::Module::CIDEntry& entry);

        nsCOMPtr<xpcIJSGetFactory> getfactoryobj;
        JSObject            *global;
        char                *location;
    };

    friend class ModuleEntry;

    // Modules are intentionally leaked, but still cleared.
    static PLDHashOperator ClearModules(const nsACString& key, ModuleEntry*& entry, void* cx);
    nsDataHashtable<nsCStringHashKey, ModuleEntry*> mModules;

    nsClassHashtable<nsCStringHashKey, ModuleEntry> mImports;
    nsDataHashtable<nsCStringHashKey, ModuleEntry*> mInProgressImports;

    bool mInitialized;
};
