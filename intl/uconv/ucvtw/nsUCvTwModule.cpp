/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nspr.h"
#include "nsString.h"
#include "pratom.h"
#include "nsCOMPtr.h"
#include "nsIFactory.h"
#include "nsIRegistry.h"
#include "nsIGenericFactory.h"
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
#include "nsIModule.h"
#include "nsUCvTWCID.h"
#include "nsUCvTWDll.h"
#include "nsCRT.h"

#include "nsBIG5ToUnicode.h"
#include "nsUnicodeToBIG5.h"
#include "nsUnicodeToBIG5NoAscii.h"
#include "nsBIG5HKSCSToUnicode.h"
#include "nsUnicodeToBIG5HKSCS.h"
#include "nsUnicodeToHKSCS.h"

//----------------------------------------------------------------------------
// Global functions and data [declaration]

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

#define DECODER_NAME_BASE "Unicode Decoder-"
#define ENCODER_NAME_BASE "Unicode Encoder-"

PRUint16 g_ufBig5Mapping[] = {
#include "big5.uf"
};

PRUint16 g_utBIG5Mapping[] = {
#include "big5.ut"
};

PRUint16 g_ASCIIMapping[] = {
  0x0001, 0x0004, 0x0005, 0x0008, 0x0000, 0x0000, 0x007F, 0x0000
};

PRUint16 g_ufBig5HKSCSMapping[] = {
#include "hkscs.uf"
};

PRUint16 g_utBig5HKSCSMapping[] = {
#include "hkscs.ut"
};

NS_CONVERTER_REGISTRY_START
NS_UCONV_REG_UNREG("Big5", "Unicode" , NS_BIG5TOUNICODE_CID)
NS_UCONV_REG_UNREG("Unicode", "x-x-big5",  NS_UNICODETOBIG5NOASCII_CID)
NS_UCONV_REG_UNREG("Unicode", "Big5" , NS_UNICODETOBIG5_CID)
NS_UCONV_REG_UNREG("Big5-HKSCS", "Unicode" , NS_BIG5HKSCSTOUNICODE_CID)
NS_UCONV_REG_UNREG("Unicode", "Big5-HKSCS" , NS_UNICODETOBIG5HKSCS_CID)
NS_UCONV_REG_UNREG("Unicode", "hkscs-1" , NS_UNICODETOHKSCS_CID)
NS_CONVERTER_REGISTRY_END

NS_IMPL_NSUCONVERTERREGSELF

NS_GENERIC_FACTORY_CONSTRUCTOR(nsBIG5ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToBIG5);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToBIG5NoAscii);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBIG5HKSCSToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToBIG5HKSCS);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToHKSCS);

static const nsModuleComponentInfo components[] = 
{
  { 
    ENCODER_NAME_BASE "Big5" , NS_UNICODETOBIG5_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "Big5",
    nsUnicodeToBIG5Constructor,
    // global converter registration
    nsUConverterRegSelf, nsUConverterUnregSelf,
  },
  { 
    ENCODER_NAME_BASE "x-x-big5" , NS_UNICODETOBIG5NOASCII_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-x-big5",
    nsUnicodeToBIG5NoAsciiConstructor,
  },
  { 
    DECODER_NAME_BASE "Big5" , NS_BIG5TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "Big5",
    nsBIG5ToUnicodeConstructor ,
  },
  {
    ENCODER_NAME_BASE "Big5-HKSCS" , NS_UNICODETOBIG5HKSCS_CID,
    NS_UNICODEENCODER_CONTRACTID_BASE "Big5-HKSCS",
    nsUnicodeToBIG5HKSCSConstructor,
  },
  {
    ENCODER_NAME_BASE "hkscs-1" , NS_UNICODETOHKSCS_CID,
    NS_UNICODEENCODER_CONTRACTID_BASE "hkscs-1",
    nsUnicodeToHKSCSConstructor,
  },
  {
    DECODER_NAME_BASE "Big5-HKSCS" , NS_BIG5HKSCSTOUNICODE_CID,
    NS_UNICODEDECODER_CONTRACTID_BASE "Big5-HKSCS",
    nsBIG5HKSCSToUnicodeConstructor ,
  }
};

NS_IMPL_NSGETMODULE(nsUCvTWModule, components);

