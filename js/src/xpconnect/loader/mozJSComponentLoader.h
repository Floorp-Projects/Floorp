/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "MPL"); you may not use this file except in
 * compliance with the MPL.  You may obtain a copy of the MPL at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the MPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL
 * for the specific language governing rights and limitations under the
 * MPL.
 *
 * The Initial Developer of this code under the MPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "plhash.h"
#include "jsapi.h"
#include "nsIComponentLoader.h"
#include "nsIRegistry.h"
#include "nsISupports.h"
#include "nsIXPConnect.h"

extern const char mozJSComponentLoaderProgID[];
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
    nsresult RegisterComponentsInDir(PRInt32 when, nsIFileSpec *dir);
    JSObject *GlobalForLocation(const char *aLocation, nsIFileSpec *component);
    nsIModule *ModuleForLocation(const char *aLocation,
                                 nsIFileSpec *component);
    PRBool HasChanged(const char *registryLocation, nsIFileSpec *component);
    nsresult SetRegistryInfo(const char *registryLocation,
                             nsIFileSpec *component);

    nsCOMPtr<nsIComponentManager> mCompMgr;
    nsCOMPtr<nsIRegistry> mRegistry;
    nsCOMPtr<nsIXPConnect> mXPC;

    JSObject  *mSuperGlobal;
    JSRuntime *mRuntime;
    JSContext *mContext;
    JSObject  *mCompMgrWrapper;
    
    PLHashTable *mModules;
    PLHashTable *mGlobals;
    nsIRegistry::Key mXPCOMKey;
};
