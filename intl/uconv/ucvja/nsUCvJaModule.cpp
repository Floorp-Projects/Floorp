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
#include "nsUCVJACID.h"
#include "nsUCVJA2CID.h"
#include "nsUCVJADll.h"
#include "nsCRT.h"

#include "nsJapaneseToUnicode.h"
#include "nsUnicodeToSJIS.h"
#include "nsUnicodeToEUCJP.h"
#include "nsUnicodeToISO2022JP.h"
#include "nsUnicodeToJISx0201.h"
#include "nsUnicodeToJISx0208.h"
#include "nsUnicodeToJISx0212.h"
//----------------------------------------------------------------------------
// Global functions and data [declaration]

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

#define DECODER_NAME_BASE "Unicode Decoder-"
#define ENCODER_NAME_BASE "Unicode Encoder-"

PRUint16 g_uf0201Mapping[] = {
#include "jis0201.uf"
};
PRUint16 g_uf0201GLMapping[] = {
#include "jis0201gl.uf"
};

PRUint16 g_uf0208Mapping[] = {
#include "jis0208.uf"
};

PRUint16 g_uf0208extMapping[] = {
#include "jis0208ext.uf"
};

PRUint16 g_uf0212Mapping[] = {
#include "jis0212.uf"
};

NS_CONVERTER_REGISTRY_START
NS_UCONV_REG_UNREG("Shift_JIS", "Unicode" , NS_SJISTOUNICODE_CID)
NS_UCONV_REG_UNREG("ISO-2022-JP", "Unicode" , NS_ISO2022JPTOUNICODE_CID)
NS_UCONV_REG_UNREG("EUC-JP", "Unicode" , NS_EUCJPTOUNICODE_CID)
NS_UCONV_REG_UNREG("Unicode", "Shift_JIS" , NS_UNICODETOSJIS_CID)
NS_UCONV_REG_UNREG("Unicode", "EUC-JP" , NS_UNICODETOEUCJP_CID)
NS_UCONV_REG_UNREG("Unicode", "ISO-2022-JP" , NS_UNICODETOISO2022JP_CID)
NS_UCONV_REG_UNREG("Unicode", "jis_0201" , NS_UNICODETOJISX0201_CID)
NS_UCONV_REG_UNREG("Unicode", "jis_0208-1983" , NS_UNICODETOJISX0208_CID)
NS_UCONV_REG_UNREG("Unicode", "jis_0212-1990" , NS_UNICODETOJISX0212_CID)
NS_CONVERTER_REGISTRY_END

NS_IMPL_NSUCONVERTERREGSELF

NS_GENERIC_FACTORY_CONSTRUCTOR(nsShiftJISToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsEUCJPToUnicodeV2);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsISO2022JPToUnicodeV2);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToSJIS);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToEUCJP);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToISO2022JP);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToJISx0201);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToJISx0208);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToJISx0212);

static const nsModuleComponentInfo components[] = 
{
  { 
    DECODER_NAME_BASE "Shift_JIS" , NS_SJISTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "Shift_JIS",
    nsShiftJISToUnicodeConstructor ,
    // global converter registration
    nsUConverterRegSelf, nsUConverterUnregSelf,
  },
  { 
    DECODER_NAME_BASE "EUC-JP" , NS_EUCJPTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "EUC-JP",
    nsEUCJPToUnicodeV2Constructor ,
  },
  { 
    DECODER_NAME_BASE "ISO-2022-JP" , NS_ISO2022JPTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-2022-JP",
    nsISO2022JPToUnicodeV2Constructor ,
  },
  { 
    ENCODER_NAME_BASE "Shift_JIS" , NS_UNICODETOSJIS_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "Shift_JIS",
    nsUnicodeToSJISConstructor, 
  },
  { 
    ENCODER_NAME_BASE "EUC-JP" , NS_UNICODETOEUCJP_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "EUC-JP",
    nsUnicodeToEUCJPConstructor, 
  },
  { 
    ENCODER_NAME_BASE "ISO-2022-JP" , NS_UNICODETOISO2022JP_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "ISO-2022-JP",
    nsUnicodeToISO2022JPConstructor, 
  },
  { 
    ENCODER_NAME_BASE "jis_0201" , NS_UNICODETOJISX0201_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "jis_0201",
    nsUnicodeToJISx0201Constructor, 
  },
  { 
    ENCODER_NAME_BASE "jis_0208-1983" , NS_UNICODETOJISX0208_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "jis_0208-1983",
    nsUnicodeToJISx0208Constructor, 
  },
  { 
    ENCODER_NAME_BASE "jis_0212-1990" , NS_UNICODETOJISX0212_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "jis_0212-1990",
    nsUnicodeToJISx0212Constructor, 
  }
};

NS_IMPL_NSGETMODULE(nsUCvJAModule, components);

