/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and imitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is International
 * Business Machines Corporation. Portions created by IBM
 * Corporation are Copyright (C) 2000 International Business
 * Machines Corporation. All Rights Reserved.
 *
 * Contributor(s): IBM Corporation.
 *
 */

#include "nsP3PService.h"
#include "nsP3PUIService.h"
#include "nsP3PUI.h"

#include <nsIGenericFactory.h>


// Define a table of CIDs implemented by this module:
//   Description
//   Class ID
//   Prog ID
//   Instance creation function
static
nsModuleComponentInfo   gP3PComponents[] = {
  { NS_P3PSERVICE_CLASSNAME,
    NS_P3PSERVICE_CID,
    NS_P3PSERVICE_CONTRACTID,
    nsP3PService::Create,
    nsP3PService::CategoryRegister,
    nsP3PService::CategoryUnregister },

  { NS_P3PUISERVICE_CLASSNAME,
    NS_P3PUISERVICE_CID,
    NS_P3PUISERVICE_CONTRACTID,
    nsP3PUIService::Create},

  { NS_P3PUI_CLASSNAME,
    NS_P3PUI_CID,
    NS_P3PUI_CONTRACTID,
    nsP3PUI::Create}

};

// Implement the NSGetModule() exported function for your module
// and the entire implementation of the module object.
NS_IMPL_NSGETMODULE( "nsP3PModule", gP3PComponents )


// Define the tags that the ElementObserver will be observing.
extern
const char *            gObserveElements[] = {
  "LINK",
  nsnull
};
