/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "msgCore.h"
#include "nsISupports.h"
#include "nsCOMPtr.h"

#include "nsIFactory.h"
#include "nsIGenericFactory.h"
#include "nsIServiceManager.h"
#include "nsIModule.h"

#include "pratom.h"
#include "nsAbSyncCID.h"

/* Include all of the interfaces our factory can generate components for */
#include "nsAbSync.h"
#include "nsAbSyncPostEngine.h"

#include "nsABSyncDriver.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbSync);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbSyncDriver)

////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////

static nsModuleComponentInfo components[] =
{
  { "Addressbook Sync",
    NS_ABSYNC_SERVICE_CID,
    NS_ABSYNC_SERVICE_CONTRACTID,
    nsAbSyncConstructor },
  { "Addressbook Sync Post Engine",
    NS_ABSYNC_POST_ENGINE_CID,
    NS_ABSYNC_POST_ENGINE_CONTRACTID,
    nsAbSyncPostEngine::Create },
  { "The Address Book Sync Driver", 
    NS_ADDBOOK_SYNCDRIVER_CID,
    NS_ADDBOOK_SYNCDRIVER_CONTRACTID,
    nsAbSyncDriverConstructor }    
};

NS_IMPL_NSGETMODULE("nsAbSyncModule", components)
