/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Igor Kushnirskiy <idk@eng.sun.com>
 */
#ifndef _bcJavaComponentLoader_h
#define _bcJavaComponentLoader_h
#include "nsIComponentLoader.h"
#include "nsIModule.h"
#include "nsHashtable.h"
#include "nsCOMPtr.h"
#include "nsIFileSpec.h"
#include "nsIFile.h"
#include "nsIRegistry.h"
#include "nsSupportsArray.h"

#define BC_JAVACOMPONENTLOADER_ContractID \
"@mozilla.org/blackwood/blackconnect/java-component-loader"

/* 0d6b5198-1dd2-11b2-b2f0-ed49ba755db8 */
#define BC_JAVACOMPONENTLOADER_CID \
  { 0x0d6b5198, 0x1dd2, 0x11b2, \
     {0xb2, 0xf0, 0xed, 0x49, 0xba, 0x75, 0x5d, 0xb8 }}

#define JAVACOMPONENTTYPENAME "text/java"

class bcJavaComponentLoader : public nsIComponentLoader {
    NS_DECL_ISUPPORTS
    NS_DECL_NSICOMPONENTLOADER

    bcJavaComponentLoader();
    virtual ~bcJavaComponentLoader();
 protected:
    nsHashtable mModules;
    nsCOMPtr<nsIRegistry> mRegistry;
    nsIComponentManager* mCompMgr; // weak ref, should make it strong?
    nsRegistryKey mXPCOMKey;
    nsSupportsArray mDeferredComponents;

    nsresult RegisterComponentsInDir(PRInt32 when, nsIFile *dir);
    nsresult AttemptRegistration(nsIFile *component, PRBool deferred);
    nsIModule * ModuleForLocation(const char *registryLocation, nsIFile *component);
    PRBool HasChanged(const char *registryLocation, nsIFile *component);
    nsresult SetRegistryInfo(const char *registryLocation, nsIFile *component);

};
#endif




