/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * XPCTL : nsCtlLEModule.cpp
 *
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
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc.  Portions created by SUN are Copyright (C) 2000 SUN
 * Microsystems, Inc. All Rights Reserved.
 *
 * This module 'XPCTL Interface' is based on Pango (www.pango.org) 
 * by Red Hat Software. Portions created by Redhat are Copyright (C) 
 * 1999 Red Hat Software.
 * 
 * Contributor(s):
 *   Prabhat Hegde (prabhat.hegde@sun.com)
 */

#include "nsCOMPtr.h"
#include "nsCRT.h"

#include "nsIFactory.h"
#include "nsIGenericFactory.h"
#include "nsIModule.h"
#include "nsULE.h"
#include "nsICharsetConverterManager.h"
#include "nsUnicodeToTIS620.h"
#include "nsUnicodeToSunIndic.h"

//----------------------------------------------------------------------------
// Global functions and data [declaration]

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

#define ENCODER_NAME_BASE "Unicode Encoder-"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsULE)

// Per DLL Globals
PRInt32 g_InstanceCount = 0;
PRInt32 g_LockCount = 0;

NS_CONVERTER_REGISTRY_START
NS_UCONV_REG_UNREG_ENCODER("tis620-2", NS_UNICODETOTIS620_CID)
NS_UCONV_REG_UNREG_ENCODER("x-sun-unicode-india-0", NS_UNICODETOSUNINDIC_CID)
NS_CONVERTER_REGISTRY_END

NS_IMPL_NSUCONVERTERREGSELF

NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToTIS620);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToSunIndic);

static const nsModuleComponentInfo components[] =
{
  { ENCODER_NAME_BASE "tis620-2" , NS_UNICODETOTIS620_CID,
    NS_UNICODEENCODER_CONTRACTID_BASE "tis620-2",
    nsUnicodeToTIS620Constructor,
    nsUConverterRegSelf, nsUConverterUnregSelf },
  { ENCODER_NAME_BASE "x-sun-unicode-india-0" , NS_UNICODETOSUNINDIC_CID,
    NS_UNICODEENCODER_CONTRACTID_BASE "x-sun-unicode-india-0",
    nsUnicodeToSunIndicConstructor,
    nsUConverterRegSelf, nsUConverterUnregSelf },
  { "Unicode Layout Engine", NS_ULE_CID, NS_ULE_PROGID, 
    nsULEConstructor, NULL, NULL }
};

NS_IMPL_NSGETMODULE(nsCtlLEModule, components)
