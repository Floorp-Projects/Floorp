/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
#include "nsCRT.h"

// ucvja
#include "nsUCVJACID.h"
#include "nsUCVJA2CID.h"
#include "nsUCVJADll.h"
#include "nsJapaneseToUnicode.h"
#include "nsUnicodeToSJIS.h"
#include "nsUnicodeToEUCJP.h"
#include "nsUnicodeToISO2022JP.h"
#include "nsUnicodeToJISx0201.h"
#include "nsUnicodeToJISx0208.h"
#include "nsUnicodeToJISx0212.h"

// ucvtw2
#include "nsUCvTW2CID.h"
#include "nsUCvTW2Dll.h"
#include "nsEUCTWToUnicode.h"
#include "nsUnicodeToEUCTW.h"
#include "nsUnicodeToCNS11643p1.h"
#include "nsUnicodeToCNS11643p2.h"
#include "nsUnicodeToCNS11643p3.h"
#include "nsUnicodeToCNS11643p4.h"
#include "nsUnicodeToCNS11643p5.h"
#include "nsUnicodeToCNS11643p6.h"
#include "nsUnicodeToCNS11643p7.h"

// ucvtw
#include "nsUCvTWCID.h"
#include "nsUCvTWDll.h"
#include "nsBIG5ToUnicode.h"
#include "nsUnicodeToBIG5.h"
#include "nsUnicodeToBIG5NoAscii.h"
#include "nsBIG5HKSCSToUnicode.h"
#include "nsUnicodeToBIG5HKSCS.h"
#include "nsUnicodeToHKSCS.h"

// ucvko
#include "nsUCvKOCID.h"
#include "nsUCvKODll.h"
#include "nsEUCKRToUnicode.h"
#include "nsUnicodeToEUCKR.h"
#include "nsUnicodeToKSC5601.h"
#include "nsUnicodeToX11Johab.h"
#include "nsJohabToUnicode.h"
#include "nsUnicodeToJohab.h"
#include "nsUnicodeToJohabNoAscii.h"
#include "nsCP949ToUnicode.h"
#include "nsUnicodeToCP949.h"
#include "nsISO2022KRToUnicode.h"

//----------------------------------------------------------------------------
// Global functions and data [declaration]

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

#define DECODER_NAME_BASE "Unicode Decoder-"
#define ENCODER_NAME_BASE "Unicode Encoder-"

// ucvja
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

// ucvtw2
PRUint16 g_ufCNS1MappingTable[] = {
#include "cns_1.uf"
};

PRUint16 g_ufCNS2MappingTable[] = {
#include "cns_2.uf"
};

PRUint16 g_ufCNS3MappingTable[] = {
#include "cns3.uf"
};

PRUint16 g_ufCNS4MappingTable[] = {
#include "cns4.uf"
};

PRUint16 g_ufCNS5MappingTable[] = {
#include "cns5.uf"
};

PRUint16 g_ufCNS6MappingTable[] = {
#include "cns6.uf"
};

PRUint16 g_ufCNS7MappingTable[] = {
#include "cns7.uf"
};

PRUint16 g_utCNS1MappingTable[] = {
#include "cns_1.ut"
};

PRUint16 g_utCNS2MappingTable[] = {
#include "cns_2.ut"
};

PRUint16 g_utCNS3MappingTable[] = {
#include "cns3.ut"
};

PRUint16 g_utCNS4MappingTable[] = {
#include "cns4.ut"
};

PRUint16 g_utCNS5MappingTable[] = {
#include "cns5.ut"
};

PRUint16 g_utCNS6MappingTable[] = {
#include "cns6.ut"
};

PRUint16 g_utCNS7MappingTable[] = {
#include "cns7.ut"
};

PRUint16 g_ASCIIMappingTable[] = {
  0x0001, 0x0004, 0x0005, 0x0008, 0x0000, 0x0000, 0x007F, 0x0000
};

// ucvtw
PRUint16 g_ufBig5Mapping[] = {
#include "big5.uf"
};

PRUint16 g_utBIG5Mapping[] = {
#include "big5.ut"
};

PRUint16 g_ufBig5HKSCSMapping[] = {
#include "hkscs.uf"
};

PRUint16 g_ASCIIMapping[] = {
  0x0001, 0x0004, 0x0005, 0x0008, 0x0000, 0x0000, 0x007F, 0x0000
};

PRUint16 g_utBig5HKSCSMapping[] = {
#include "hkscs.ut"
};

// ucvko
PRUint16 g_utKSC5601Mapping[] = {
#include "u20kscgl.ut"
};

PRUint16 g_ufKSC5601Mapping[] = {
#include "u20kscgl.uf"
};

PRUint16 g_AsciiMapping[] = {
  0x0001, 0x0004, 0x0005, 0x0008, 0x0000, 0x0000, 0x007F, 0x0000
};
PRUint16 g_HangulNullMapping[] ={
  0x0001, 0x0004, 0x0005, 0x0008, 0x0000, 0xAC00, 0xD7A3, 0xAC00
};
PRUint16 g_ufJohabJamoMapping[] ={   
#include "johabjamo.uf"
};

NS_CONVERTER_REGISTRY_START
    // ucvja
NS_UCONV_REG_UNREG("Shift_JIS", NS_SJISTOUNICODE_CID, NS_UNICODETOSJIS_CID)
NS_UCONV_REG_UNREG("ISO-2022-JP", NS_ISO2022JPTOUNICODE_CID, NS_UNICODETOISO2022JP_CID)
NS_UCONV_REG_UNREG("EUC-JP", NS_EUCJPTOUNICODE_CID, NS_UNICODETOEUCJP_CID)
  
NS_UCONV_REG_UNREG_ENCODER("jis_0201" , NS_UNICODETOJISX0201_CID)
NS_UCONV_REG_UNREG_ENCODER("jis_0208-1983" , NS_UNICODETOJISX0208_CID)
NS_UCONV_REG_UNREG_ENCODER("jis_0212-1990" , NS_UNICODETOJISX0212_CID)

    // ucvtw2
NS_UCONV_REG_UNREG("x-euc-tw", NS_EUCTWTOUNICODE_CID, NS_UNICODETOEUCTW_CID)
NS_UCONV_REG_UNREG_ENCODER("x-cns-11643-1" , NS_UNICODETOCNS11643P1_CID)
NS_UCONV_REG_UNREG_ENCODER("x-cns-11643-2" , NS_UNICODETOCNS11643P2_CID)
NS_UCONV_REG_UNREG_ENCODER("x-cns-11643-3" , NS_UNICODETOCNS11643P3_CID)
NS_UCONV_REG_UNREG_ENCODER("x-cns-11643-4" , NS_UNICODETOCNS11643P4_CID)
NS_UCONV_REG_UNREG_ENCODER("x-cns-11643-5" , NS_UNICODETOCNS11643P5_CID)
NS_UCONV_REG_UNREG_ENCODER("x-cns-11643-6" , NS_UNICODETOCNS11643P6_CID)
NS_UCONV_REG_UNREG_ENCODER("x-cns-11643-7" , NS_UNICODETOCNS11643P7_CID)

    // ucvtw
NS_UCONV_REG_UNREG("Big5", NS_BIG5TOUNICODE_CID, NS_UNICODETOBIG5_CID)
NS_UCONV_REG_UNREG("Big5-HKSCS", NS_BIG5HKSCSTOUNICODE_CID, NS_UNICODETOBIG5HKSCS_CID)
  
NS_UCONV_REG_UNREG_ENCODER("hkscs-1" , NS_UNICODETOHKSCS_CID)
NS_UCONV_REG_UNREG_ENCODER("x-x-big5",  NS_UNICODETOBIG5NOASCII_CID)

    // ucvko
NS_UCONV_REG_UNREG("EUC-KR", NS_EUCKRTOUNICODE_CID, NS_UNICODETOEUCKR_CID)
NS_UCONV_REG_UNREG("x-johab", NS_JOHABTOUNICODE_CID, NS_UNICODETOJOHAB_CID)
NS_UCONV_REG_UNREG("x-windows-949", NS_CP949TOUNICODE_CID, NS_UNICODETOCP949_CID)
NS_UCONV_REG_UNREG_DECODER("ISO-2022-KR", NS_ISO2022KRTOUNICODE_CID)

NS_UCONV_REG_UNREG_ENCODER("ks_c_5601-1987",  NS_UNICODETOKSC5601_CID)
NS_UCONV_REG_UNREG_ENCODER("x-x11johab",  NS_UNICODETOX11JOHAB_CID)
NS_UCONV_REG_UNREG_ENCODER("x-johab-noascii",  NS_UNICODETOJOHABNOASCII_CID)

NS_CONVERTER_REGISTRY_END

NS_IMPL_NSUCONVERTERREGSELF

// ucvja
NS_GENERIC_FACTORY_CONSTRUCTOR(nsShiftJISToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsEUCJPToUnicodeV2);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsISO2022JPToUnicodeV2);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToSJIS);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToEUCJP);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToISO2022JP);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToJISx0201);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToJISx0208);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToJISx0212);

// ucvtw2
NS_GENERIC_FACTORY_CONSTRUCTOR(nsEUCTWToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToEUCTW);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToCNS11643p1);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToCNS11643p2);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToCNS11643p3);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToCNS11643p4);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToCNS11643p5);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToCNS11643p6);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToCNS11643p7);

// ucvtw
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBIG5ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToBIG5);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToBIG5NoAscii);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBIG5HKSCSToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToBIG5HKSCS);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToHKSCS);

// ucvko
NS_GENERIC_FACTORY_CONSTRUCTOR(nsEUCKRToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToEUCKR);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToKSC5601);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToX11Johab);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsJohabToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToJohab);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToJohabNoAscii);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCP949ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToCP949);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsISO2022KRToUnicode);


static const nsModuleComponentInfo components[] = 
{
    // ucvja
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
  },

  // ucvtw2
  { 
    DECODER_NAME_BASE "x-euc-tw" , NS_EUCTWTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-euc-tw",
    nsEUCTWToUnicodeConstructor,
    // global converter registration
    nsUConverterRegSelf, nsUConverterUnregSelf
  },
  { 
    ENCODER_NAME_BASE "x-euc-tw" , NS_UNICODETOEUCTW_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-euc-tw",
    nsUnicodeToEUCTWConstructor,
  },
  { 
    ENCODER_NAME_BASE "x-cns-11643-1" , NS_UNICODETOCNS11643P1_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-cns-11643-1",
    nsUnicodeToCNS11643p1Constructor,
  },
  { 
    ENCODER_NAME_BASE "x-cns-11643-2" , NS_UNICODETOCNS11643P2_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-cns-11643-2",
    nsUnicodeToCNS11643p2Constructor,
  },
  { 
    ENCODER_NAME_BASE "x-cns-11643-3" , NS_UNICODETOCNS11643P3_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-cns-11643-3",
    nsUnicodeToCNS11643p3Constructor,
  },
  { 
    ENCODER_NAME_BASE "x-cns-11643-4" , NS_UNICODETOCNS11643P4_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-cns-11643-4",
    nsUnicodeToCNS11643p4Constructor,
  },
  { 
    ENCODER_NAME_BASE "x-cns-11643-5" , NS_UNICODETOCNS11643P5_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-cns-11643-5",
    nsUnicodeToCNS11643p5Constructor,
  },
  { 
    ENCODER_NAME_BASE "x-cns-11643-6" , NS_UNICODETOCNS11643P6_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-cns-11643-6",
    nsUnicodeToCNS11643p6Constructor,
  },
  { 
    ENCODER_NAME_BASE "x-cns-11643-7" , NS_UNICODETOCNS11643P7_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-cns-11643-7",
    nsUnicodeToCNS11643p7Constructor,
  },

  // ucvtw
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
  },

  // ucvko
  { 
    DECODER_NAME_BASE "EUC-KR" , NS_EUCKRTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "EUC-KR",
    nsEUCKRToUnicodeConstructor ,
    // global converter registration
    nsUConverterRegSelf, nsUConverterUnregSelf,
  },
  { 
    ENCODER_NAME_BASE "EUC-KR" , NS_UNICODETOEUCKR_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "EUC-KR",
    nsUnicodeToEUCKRConstructor, 
  },
  { 
    ENCODER_NAME_BASE "ks_c_5601-1987" , NS_UNICODETOKSC5601_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "ks_c_5601-1987",
    nsUnicodeToKSC5601Constructor,
  },
  { 
    ENCODER_NAME_BASE "x-x11johab" , NS_UNICODETOX11JOHAB_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-x11johab",
    nsUnicodeToX11JohabConstructor,
  },
  { 
    DECODER_NAME_BASE "x-johab" , NS_JOHABTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-johab",
    nsJohabToUnicodeConstructor ,
  },
  { 
    ENCODER_NAME_BASE "x-johab" , NS_UNICODETOJOHAB_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-johab",
    nsUnicodeToJohabConstructor,
  },
  { 
    ENCODER_NAME_BASE "x-johab-noascii", NS_UNICODETOJOHABNOASCII_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-johab-noascii",
    nsUnicodeToJohabNoAsciiConstructor,
  },
  { 
    DECODER_NAME_BASE "x-windows-949" , NS_CP949TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-windows-949",
    nsCP949ToUnicodeConstructor ,
  },
  { 
    ENCODER_NAME_BASE "x-windows-949" , NS_UNICODETOCP949_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-windows-949",
    nsUnicodeToCP949Constructor,
  },
  { 
    DECODER_NAME_BASE "ISO-2022-KR" , NS_ISO2022KRTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-2022-KR",
    nsISO2022KRToUnicodeConstructor ,
  }
};

NS_IMPL_NSGETMODULE(nsUCvAsiaModule, components);

