/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "stdio.h"
#include "xp_core.h"                    //this is a hack to get it to build. MMP
#include "nscore.h"
#include "nsIFactory.h"
#include "nsIGenericFactory.h"     // need this for ps code
#include "nsISupports.h"
#include "nsGfxCIID.h"
#include "nsIModule.h"
#include "nsCOMPtr.h"
#include "nsDeviceContextXP.h"
#include "nsGfxXPrintCID.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceContextXP)

static nsModuleComponentInfo components[] =
{
  { "GFX Postscript Device Context",
    NS_DEVICECONTEXTXP_CID,
    "@mozilla.org/gfx/decidecontext/xprt;1",
    nsDeviceContextXPConstructor }  
};

NS_IMPL_NSGETMODULE("nsGfxXPModule", components)
