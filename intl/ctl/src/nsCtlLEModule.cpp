/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * XPCTL : nsCtlLEModule.cpp
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Prabhat Hegde (prabhat.hegde@sun.com)
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

#include "nsCOMPtr.h"
#include "nsCRT.h"

#include "nsIFactory.h"
#include "nsIGenericFactory.h"
#include "nsIModule.h"
#include "nsULE.h"
#include "nsICategoryManager.h"
#include "nsEncoderDecoderUtils.h"
#include "nsUnicodeToTIS620.h"
#include "nsUnicodeToSunIndic.h"
#include "nsUnicodeToThaiTTF.h"

//----------------------------------------------------------------------------
// Global functions and data [declaration]

#define ENCODER_NAME_BASE "Unicode Encoder-"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsULE)

// Per DLL Globals
PRInt32 g_InstanceCount = 0;
PRInt32 g_LockCount = 0;

NS_CONVERTER_REGISTRY_START
NS_UCONV_REG_UNREG_ENCODER("tis620-2", NS_UNICODETOTIS620_CID)
NS_UCONV_REG_UNREG_ENCODER("x-thaittf-0", NS_UNICODETOTHAITTF_CID)
NS_UCONV_REG_UNREG_ENCODER("x-sun-unicode-india-0", NS_UNICODETOSUNINDIC_CID)
NS_CONVERTER_REGISTRY_END

NS_IMPL_NSUCONVERTERREGSELF

NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToTIS620)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToThaiTTF)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToSunIndic)

static const nsModuleComponentInfo components[] =
{
  { ENCODER_NAME_BASE "tis620-2" , NS_UNICODETOTIS620_CID,
    NS_UNICODEENCODER_CONTRACTID_BASE "tis620-2",
    nsUnicodeToTIS620Constructor,
    nsUConverterRegSelf, nsUConverterUnregSelf },
  { ENCODER_NAME_BASE "x-thaittf-0" , NS_UNICODETOTHAITTF_CID,
    NS_UNICODEENCODER_CONTRACTID_BASE "x-thaittf-0",
    nsUnicodeToThaiTTFConstructor,
    nsUConverterRegSelf, nsUConverterUnregSelf },
  { ENCODER_NAME_BASE "x-sun-unicode-india-0" , NS_UNICODETOSUNINDIC_CID,
    NS_UNICODEENCODER_CONTRACTID_BASE "x-sun-unicode-india-0",
    nsUnicodeToSunIndicConstructor,
    nsUConverterRegSelf, nsUConverterUnregSelf },
  { "Unicode Layout Engine", NS_ULE_CID, NS_ULE_PROGID, 
    nsULEConstructor, NULL, NULL }
};

NS_IMPL_NSGETMODULE(nsCtlLEModule, components)
