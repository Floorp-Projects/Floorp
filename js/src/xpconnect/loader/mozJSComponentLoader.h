/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#include "nsIComponentLoader.h"
#include "nsIComponentLoaderManager.h"
#include "nsIJSRuntimeService.h"
#include "nsIJSContextStack.h"
#include "nsISupports.h"
#include "nsIXPConnect.h"
#include "nsIModule.h"
#include "nsSupportsArray.h"
#include "nsIFile.h"
#ifndef XPCONNECT_STANDALONE
#include "nsIPrincipal.h"
#endif
extern const char mozJSComponentLoaderContractID[];
extern const char jsComponentTypeName[];

/* 6bd13476-1dd2-11b2-bbef-f0ccb5fa64b6 (thanks, mozbot) */

#define MOZJSCOMPONENTLOADER_CID \
  {0x6bd13476, 0x1dd2, 0x11b2, \
    { 0xbb, 0xef, 0xf0, 0xcc, 0xb5, 0xfa, 0x64, 0xb6 }}

class mozJSComponentLoader : public nsIComponentLoader {

public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICOMPONENTLOADER

    mozJSComponentLoader();
    virtual ~mozJSComponentLoader();

 protected:
    nsresult ReallyInit();
    nsresult AttemptRegistration(nsIFile *component, PRBool deferred);
    nsresult UnregisterComponent(nsIFile *component);
    nsresult RegisterComponentsInDir(PRInt32 when, nsIFile *dir);
    JSObject *GlobalForLocation(const char *aLocation, nsIFile *component);
    nsIModule *ModuleForLocation(const char *aLocation, nsIFile *component);
    PRBool HasChanged(const char *registryLocation, nsIFile *component);
    nsresult SetRegistryInfo(const char *registryLocation, nsIFile *component);
    nsresult RemoveRegistryInfo(nsIFile *component, const char *registryLocation);

    nsCOMPtr<nsIComponentManager> mCompMgr;
    nsCOMPtr<nsIComponentLoaderManager> mLoaderManager;
    nsCOMPtr<nsIJSRuntimeService> mRuntimeService;
#ifndef XPCONNECT_STANDALONE
    nsCOMPtr<nsIPrincipal> mSystemPrincipal;
#endif
    JSRuntime *mRuntime;
    PLHashTable *mModules;
    PLHashTable *mGlobals;

    PRBool mInitialized;
    nsSupportsArray mDeferredComponents;
};

class JSCLAutoContext
{
public:
    JSCLAutoContext(JSRuntime* rt);
    ~JSCLAutoContext();

    operator JSContext*() const {return mContext;}
    JSContext* GetContext() const {return mContext;}
    nsresult   GetError()   const {return mError;}


    JSCLAutoContext(); // not implemnted
private:
    JSContext* mContext;
    nsresult   mError;
    JSBool     mPopNeeded;
    intN       mContextThread; 
};


class JSCLAutoErrorReporterSetter
{
public:
    JSCLAutoErrorReporterSetter(JSContext* cx, JSErrorReporter reporter)
        {mContext = cx; mOldReporter = JS_SetErrorReporter(cx, reporter);}
    ~JSCLAutoErrorReporterSetter()
        {JS_SetErrorReporter(mContext, mOldReporter);} 
    JSCLAutoErrorReporterSetter(); // not implemented
private:
    JSContext* mContext;
    JSErrorReporter mOldReporter;
};
