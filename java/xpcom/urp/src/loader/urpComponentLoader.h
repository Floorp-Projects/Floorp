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
 * Sergey Lunegov <lsv@sparc.spb.su>
 */
#ifndef _urpComponentLoader_h
#define _urpComponentLoader_h
#include "nsIComponentLoader.h"
#include "nsIModule.h"
#include "nsHashtable.h"
#include "nsCOMPtr.h"
#include "nsIFileSpec.h"
#include "nsIFile.h"
#include "nsIRegistry.h"
#include "nsSupportsArray.h"

#define URP_COMPONENTLOADER_ContractID \
"@mozilla.org/blackwood/starconnect/remote-component-loader;1"

/* 0d6b5198-1dd2-11b2-b2f0-ed49ba755db8 */
#define URP_COMPONENTLOADER_CID \
  { 0xdcc5322f, 0x06e5, 0x4a3f, \
     {0x92, 0xe8, 0xde, 0xe6, 0x8f, 0x26, 0x0d, 0x8f }}

#define URPCOMPONENTTYPENAME "application/remote-component"

class urpComponentLoader : public nsIComponentLoader {
    NS_DECL_ISUPPORTS
    NS_DECL_NSICOMPONENTLOADER

    urpComponentLoader();
    virtual ~urpComponentLoader();
 protected:
    nsCOMPtr<nsIRegistry> mRegistry;
    nsCOMPtr<nsIComponentManager> mCompMgr; 
    nsRegistryKey mXPCOMKey;
    nsSupportsArray mDeferredComponents;

    nsresult RegisterComponentsInDir(PRInt32 when, nsIFile *dir);
    nsresult AttemptRegistration(nsIFile *component, PRBool deferred);
    PRBool HasChanged(const char *registryLocation, nsIFile *component);
    nsresult SetRegistryInfo(const char *registryLocation, nsIFile *component);

};
#endif




