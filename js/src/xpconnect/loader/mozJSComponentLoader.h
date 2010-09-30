/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributors:
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "plhash.h"
#include "jsapi.h"
#include "mozilla/ModuleLoader.h"
#include "nsIJSRuntimeService.h"
#include "nsIJSContextStack.h"
#include "nsISupports.h"
#include "nsIXPConnect.h"
#include "nsIFile.h"
#include "nsAutoPtr.h"
#include "nsIFastLoadService.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsITimer.h"
#include "nsIObserver.h"
#include "xpcIJSModuleLoader.h"
#include "nsClassHashtable.h"
#include "nsDataHashtable.h"
#ifndef XPCONNECT_STANDALONE
#include "nsIPrincipal.h"
#endif
#ifdef MOZ_ENABLE_LIBXUL
#include "mozilla/scache/StartupCache.h"

using namespace mozilla::scache;
#endif

#include "xpcIJSGetFactory.h"

/* 6bd13476-1dd2-11b2-bbef-f0ccb5fa64b6 (thanks, mozbot) */

#define MOZJSCOMPONENTLOADER_CID \
  {0x6bd13476, 0x1dd2, 0x11b2, \
    { 0xbb, 0xef, 0xf0, 0xcc, 0xb5, 0xfa, 0x64, 0xb6 }}
#define MOZJSCOMPONENTLOADER_CONTRACTID "@mozilla.org/moz/jsloader;1"

// nsIFastLoadFileIO implementation for component fastload
class nsXPCFastLoadIO : public nsIFastLoadFileIO
{
 public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIFASTLOADFILEIO

    nsXPCFastLoadIO(nsIFile *file) : mFile(file), mTruncateOutputFile(true) {}

    void SetInputStream(nsIInputStream *stream) { mInputStream = stream; }
    void SetOutputStream(nsIOutputStream *stream) { mOutputStream = stream; }

 private:
    ~nsXPCFastLoadIO() {}

    nsCOMPtr<nsIFile> mFile;
    nsCOMPtr<nsIInputStream> mInputStream;
    nsCOMPtr<nsIOutputStream> mOutputStream;
    bool mTruncateOutputFile;
};


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
    const mozilla::Module* LoadModule(nsILocalFile* aFile);
    const mozilla::Module* LoadModuleFromJAR(nsILocalFile* aJARFile,
                                             const nsACString& aPath);

 protected:
    static mozJSComponentLoader* sSelf;

    nsresult ReallyInit();
    void UnloadModules();

    nsresult FileKey(nsILocalFile* aFile, nsAString &aResult);
    nsresult JarKey(nsILocalFile* aFile,
                    const nsACString& aComponentPath,
                    nsAString &aResult);

    const mozilla::Module* LoadModuleImpl(nsILocalFile* aSourceFile,
                                          nsAString &aKey,
                                          nsIURI* aComponentURI);

    nsresult GlobalForLocation(nsILocalFile* aComponentFile,
                               nsIURI *aComponent,
                               JSObject **aGlobal,
                               char **location,
                               jsval *exception);

#ifdef MOZ_ENABLE_LIBXUL
    nsresult ReadScript(StartupCache *cache, nsIURI *uri, 
                        JSContext *cx, JSScript **script);
    nsresult WriteScript(StartupCache *cache, JSScript *script,
                         nsIFile *component, nsIURI *uri, JSContext *cx);
#endif

    nsCOMPtr<nsIComponentManager> mCompMgr;
    nsCOMPtr<nsIJSRuntimeService> mRuntimeService;
    nsCOMPtr<nsIThreadJSContextStack> mContextStack;
#ifndef XPCONNECT_STANDALONE
    nsCOMPtr<nsIPrincipal> mSystemPrincipal;
#endif
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

            global = nsnull;
            location = nsnull;
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
    static PLDHashOperator ClearModules(const nsAString& key, ModuleEntry*& entry, void* cx);
    nsDataHashtable<nsStringHashKey, ModuleEntry*> mModules;

    nsClassHashtable<nsStringHashKey, ModuleEntry> mImports;
    nsDataHashtable<nsStringHashKey, ModuleEntry*> mInProgressImports;

    PRBool mInitialized;
};
