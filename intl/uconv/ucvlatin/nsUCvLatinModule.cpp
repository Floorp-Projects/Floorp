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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#define NS_IMPL_IDS

#include "pratom.h"
#include "nspr.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIFactory.h"
#include "nsIRegistry.h"
#include "nsIServiceManager.h"
#include "nsIModule.h"
#include "nsUCvLatinCID.h"
#include "nsUCvLatinDll.h"

#include "nsUEscapeToUnicode.h"
#include "nsUnicodeToUEscape.h"
#include "nsAsciiToUnicode.h"
#include "nsISO88591ToUnicode.h"
#include "nsISO88592ToUnicode.h"
#include "nsISO88593ToUnicode.h"
#include "nsISO88594ToUnicode.h"
#include "nsISO88595ToUnicode.h"
#include "nsISO88596ToUnicode.h"
#include "nsISO88597ToUnicode.h"
#include "nsISO88598ToUnicode.h"
#include "nsISO88599ToUnicode.h"
#include "nsISO885910ToUnicode.h"
#include "nsISO885913ToUnicode.h"
#include "nsISO885914ToUnicode.h"
#include "nsISO885915ToUnicode.h"
#include "nsISOIR111ToUnicode.h"
#include "nsCP1250ToUnicode.h"
#include "nsCP1251ToUnicode.h"
#include "nsCP1252ToUnicode.h"
#include "nsCP1253ToUnicode.h"
#include "nsCP1254ToUnicode.h"
#include "nsCP1255ToUnicode.h"
#include "nsCP1256ToUnicode.h"
#include "nsCP1257ToUnicode.h"
#include "nsCP1258ToUnicode.h"
#include "nsCP874ToUnicode.h"
#include "nsCP866ToUnicode.h"
#include "nsKOI8RToUnicode.h"
#include "nsKOI8UToUnicode.h"
#include "nsMacRomanToUnicode.h"
#include "nsMacCEToUnicode.h"
#include "nsMacGreekToUnicode.h"
#include "nsMacTurkishToUnicode.h"
#include "nsMacCroatianToUnicode.h"
#include "nsMacRomanianToUnicode.h"
#include "nsMacCyrillicToUnicode.h"
#include "nsMacUkrainianToUnicode.h"
#include "nsMacIcelandicToUnicode.h"
#include "nsARMSCII8ToUnicode.h"
#include "nsTCVN5712ToUnicode.h"
#include "nsVISCIIToUnicode.h"
#include "nsVPSToUnicode.h"
#include "nsUTF8ToUnicode.h"
#include "nsUTF7ToUnicode.h"
#include "nsMUTF7ToUnicode.h"
#include "nsUCS4BEToUnicode.h"
#include "nsUCS4LEToUnicode.h"
#include "nsUCS2BEToUnicode.h"
#include "nsUCS2LEToUnicode.h"
#include "nsT61ToUnicode.h"
#include "nsUserDefinedToUnicode.h"
#include "nsUnicodeToAscii.h"
#include "nsUnicodeToISO88591.h"
#include "nsUnicodeToISO88592.h"
#include "nsUnicodeToISO88593.h"
#include "nsUnicodeToISO88594.h"
#include "nsUnicodeToISO88595.h"
#include "nsUnicodeToISO88596.h"
#include "nsUnicodeToISO88597.h"
#include "nsUnicodeToISO88598.h"
#include "nsUnicodeToISO88599.h"
#include "nsUnicodeToISO885910.h"
#include "nsUnicodeToISO885913.h"
#include "nsUnicodeToISO885914.h"
#include "nsUnicodeToISO885915.h"
#include "nsUnicodeToISOIR111.h"
#include "nsUnicodeToCP1250.h"
#include "nsUnicodeToCP1251.h"
#include "nsUnicodeToCP1252.h"
#include "nsUnicodeToCP1253.h"
#include "nsUnicodeToCP1254.h"
#include "nsUnicodeToCP1255.h"
#include "nsUnicodeToCP1256.h"
#include "nsUnicodeToCP1257.h"
#include "nsUnicodeToCP1258.h"
#include "nsUnicodeToCP874.h"
#include "nsUnicodeToCP866.h"
#include "nsUnicodeToKOI8R.h"
#include "nsUnicodeToKOI8U.h"
#include "nsUnicodeToMacRoman.h"
#include "nsUnicodeToMacCE.h"
#include "nsUnicodeToMacGreek.h"
#include "nsUnicodeToMacTurkish.h"
#include "nsUnicodeToMacCroatian.h"
#include "nsUnicodeToMacRomanian.h"
#include "nsUnicodeToMacCyrillic.h"
#include "nsUnicodeToMacUkrainian.h"
#include "nsUnicodeToMacIcelandic.h"
#include "nsUnicodeToARMSCII8.h"
#include "nsUnicodeToTCVN5712.h"
#include "nsUnicodeToVISCII.h"
#include "nsUnicodeToVPS.h"
#include "nsUnicodeToUTF8.h"
#include "nsUnicodeToUTF7.h"
#include "nsUnicodeToMUTF7.h"
#include "nsUnicodeToUCS2BE.h"
#include "nsUnicodeToUCS2LE.h"
#include "nsUnicodeToUCS4BE.h"
#include "nsUnicodeToUCS4LE.h"
#include "nsUnicodeToT61.h"
#include "nsUnicodeToUserDefined.h"
#include "nsUnicodeToSymbol.h"
#include "nsUnicodeToZapfDingbat.h"

//----------------------------------------------------------------------------
// Global functions and data [declaration]

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

#define DECODER_NAME_BASE "Unicode Decoder-"
#define ENCODER_NAME_BASE "Unicode Encoder-"

PRInt32 g_InstanceCount = 0;
PRInt32 g_LockCount = 0;

typedef nsresult (* fpCreateInstance) (nsISupports **);

struct FactoryData
{
  const nsCID   * mCID;
  fpCreateInstance  CreateInstance;
  char    * mCharsetSrc;
  char    * mCharsetDest;
};

static FactoryData g_FactoryData[] =
{
  {
    &kAsciiToUnicodeCID,
    nsAsciiToUnicode::CreateInstance,
    "us-ascii",
    "Unicode"
  },
  {
    &kUEscapeToUnicodeCID,
    NEW_UEscapeToUnicode,
    "x-u-escaped",
    "Unicode"
  },
  {
    &kISO88591ToUnicodeCID,
    nsISO88591ToUnicode::CreateInstance,
    "ISO-8859-1",
    "Unicode"
  },
  {
    &kISO88592ToUnicodeCID,
    nsISO88592ToUnicode::CreateInstance,
    "ISO-8859-2",
    "Unicode"
  },
  {
    &kISO88593ToUnicodeCID,
    nsISO88593ToUnicode::CreateInstance,
    "ISO-8859-3",
    "Unicode"
  },
  {
    &kISO88594ToUnicodeCID,
    nsISO88594ToUnicode::CreateInstance,
    "ISO-8859-4",
    "Unicode"
  },
  {
    &kISO88595ToUnicodeCID,
    nsISO88595ToUnicode::CreateInstance,
    "ISO-8859-5",
    "Unicode"
  },
  {
    &kISO88596ToUnicodeCID,
    nsISO88596ToUnicode::CreateInstance,
    "ISO-8859-6",
    "Unicode"
  },
  {
    &kISO88596IToUnicodeCID,
    nsISO88596ToUnicode::CreateInstance,
    "ISO-8859-6-I",
    "Unicode"
  },
  {
    &kISO88596EToUnicodeCID,
    nsISO88596ToUnicode::CreateInstance,
    "ISO-8859-6-E",
    "Unicode"
  },
  {
    &kISO88597ToUnicodeCID,
    nsISO88597ToUnicode::CreateInstance,
    "ISO-8859-7",
    "Unicode"
  },
  {
    &kISO88598ToUnicodeCID,
    nsISO88598ToUnicode::CreateInstance,
    "ISO-8859-8",
    "Unicode"
  },
  {
    &kISO88598IToUnicodeCID,
    nsISO88598ToUnicode::CreateInstance,
    "ISO-8859-8-I",
    "Unicode"
  },
  {
    &kISO88598EToUnicodeCID,
    nsISO88598ToUnicode::CreateInstance,
    "ISO-8859-8-E",
    "Unicode"
  },
  {
    &kISO88599ToUnicodeCID,
    nsISO88599ToUnicode::CreateInstance,
    "ISO-8859-9",
    "Unicode"
  },
  {
    &kISO885910ToUnicodeCID,
    nsISO885910ToUnicode::CreateInstance,
    "ISO-8859-10",
    "Unicode"
  },
  {
    &kISO885913ToUnicodeCID,
    nsISO885913ToUnicode::CreateInstance,
    "ISO-8859-13",
    "Unicode"
  },
  {
    &kISO885914ToUnicodeCID,
    nsISO885914ToUnicode::CreateInstance,
    "ISO-8859-14",
    "Unicode"
  },
  {
    &kISO885915ToUnicodeCID,
    nsISO885915ToUnicode::CreateInstance,
    "ISO-8859-15",
    "Unicode"
  },
  {
    &kISOIR111ToUnicodeCID,
    nsISOIR111ToUnicode::CreateInstance,
    "ISO-IR-111",
    "Unicode"
  },
  {
    &kCP1250ToUnicodeCID,
    nsCP1250ToUnicode::CreateInstance,
    "windows-1250",
    "Unicode"
  },
  {
    &kCP1251ToUnicodeCID,
    nsCP1251ToUnicode::CreateInstance,
    "windows-1251",
    "Unicode"
  },
  {
    &kCP1252ToUnicodeCID,
    nsCP1252ToUnicode::CreateInstance,
    "windows-1252",
    "Unicode"
  },
  {
    &kCP1253ToUnicodeCID,
    nsCP1253ToUnicode::CreateInstance,
    "windows-1253",
    "Unicode"
  },
  {
    &kCP1254ToUnicodeCID,
    nsCP1254ToUnicode::CreateInstance,
    "windows-1254",
    "Unicode"
  },
  {
    &kCP1255ToUnicodeCID,
    nsCP1255ToUnicode::CreateInstance,
    "windows-1255",
    "Unicode"
  },
  {
    &kCP1256ToUnicodeCID,
    nsCP1256ToUnicode::CreateInstance,
    "windows-1256",
    "Unicode"
  },
  {
    &kCP1257ToUnicodeCID,
    nsCP1257ToUnicode::CreateInstance,
    "windows-1257",
    "Unicode"
  },
  {
    &kCP1258ToUnicodeCID,
    nsCP1258ToUnicode::CreateInstance,
    "windows-1258",
    "Unicode"
  },
  {
    &kCP874ToUnicodeCID,
    nsCP874ToUnicode::CreateInstance,
    "TIS-620",
    "Unicode"
  },
  {
    &kCP866ToUnicodeCID,
    nsCP866ToUnicode::CreateInstance,
    "IBM866",
    "Unicode"
  },
  {
    &kKOI8RToUnicodeCID,
    nsKOI8RToUnicode::CreateInstance,
    "KOI8-R",
    "Unicode"
  },
  {
    &kKOI8UToUnicodeCID,
    nsKOI8UToUnicode::CreateInstance,
    "KOI8-U",
    "Unicode"
  },
  {
    &kMacRomanToUnicodeCID,
    nsMacRomanToUnicode::CreateInstance,
    "x-mac-roman",
    "Unicode"
  },
  {
    &kMacCEToUnicodeCID,
    nsMacCEToUnicode::CreateInstance,
    "x-mac-ce",
    "Unicode"
  },
  {
    &kMacGreekToUnicodeCID,
    nsMacGreekToUnicode::CreateInstance,
    "x-mac-greek",
    "Unicode"
  },
  {
    &kMacTurkishToUnicodeCID,
    nsMacTurkishToUnicode::CreateInstance,
    "x-mac-turkish",
    "Unicode"
  },
  {
    &kMacCroatianToUnicodeCID,
    nsMacCroatianToUnicode::CreateInstance,
    "x-mac-croatian",
    "Unicode"
  },
  {
    &kMacRomanianToUnicodeCID,
    nsMacRomanianToUnicode::CreateInstance,
    "x-mac-romanian",
    "Unicode"
  },
  {
    &kMacCyrillicToUnicodeCID,
    nsMacCyrillicToUnicode::CreateInstance,
    "x-mac-cyrillic",
    "Unicode"
  },
  {
    &kMacUkrainianToUnicodeCID,
    nsMacUkrainianToUnicode::CreateInstance,
    "x-mac-ukrainian",
    "Unicode"
  },
  {
    &kMacIcelandicToUnicodeCID,
    nsMacIcelandicToUnicode::CreateInstance,
    "x-mac-icelandic",
    "Unicode"
  },
  {
    &kARMSCII8ToUnicodeCID,
    nsARMSCII8ToUnicode::CreateInstance,
    "armscii-8",
    "Unicode"
  },
  {
    &kTCVN5712ToUnicodeCID,
    nsTCVN5712ToUnicode::CreateInstance,
    "x-viet-tcvn5712",
    "Unicode"
  },
  {
    &kVISCIIToUnicodeCID,
    nsVISCIIToUnicode::CreateInstance,
    "VISCII",
    "Unicode"
  },
  {
    &kVPSToUnicodeCID,
    nsVPSToUnicode::CreateInstance,
    "x-viet-vps",
    "Unicode"
  },
  {
    &kUTF8ToUnicodeCID,
    nsUTF8ToUnicode::CreateInstance,
    "UTF-8",
    "Unicode"
  },
  {
    &kUTF7ToUnicodeCID,
    nsUTF7ToUnicode::CreateInstance,
    "UTF-7",
    "Unicode"
  },
  {
    &kMUTF7ToUnicodeCID,
    nsMUTF7ToUnicode::CreateInstance,
    "x-imap4-modified-utf7",
    "Unicode"
  },
  {
    &kUTF16BEToUnicodeCID,
    //nsUCS2BEToUnicode::CreateInstance,
    NEW_UTF16BEToUnicode,
    "UTF-16BE",
    "Unicode"
  },
  {
    &kUTF16LEToUnicodeCID,
    //nsUCS2LEToUnicode::CreateInstance,
    NEW_UTF16LEToUnicode,
    "UTF-16LE",
    "Unicode"
  },
  {
    &kUTF32BEToUnicodeCID,
    nsUCS4BEToUnicode::CreateInstance,
    "UTF-32BE",
    "Unicode"
  },
  {
    &kUTF32LEToUnicodeCID,
    nsUCS4LEToUnicode::CreateInstance,
    "UTF-32LE",
    "Unicode"
  },
  {
    &kT61ToUnicodeCID,
    nsT61ToUnicode::CreateInstance,
    "T.61-8bit",
    "Unicode"
  },
  {
    &kUserDefinedToUnicodeCID,
    nsUserDefinedToUnicode::CreateInstance,
    "x-user-defined",
    "Unicode"
  },
  {
    &kUnicodeToAsciiCID,
    nsUnicodeToAscii::CreateInstance,
    "Unicode",
    "us-ascii"
  },
  {
    &kUnicodeToUEscapeCID,
    NEW_UnicodeToUEscape,
    "Unicode",
    "x-u-escaped"
  },
  {
    &kUnicodeToISO88591CID,
    nsUnicodeToISO88591::CreateInstance,
    "Unicode",
    "ISO-8859-1"
  },
  {
    &kUnicodeToISO88592CID,
    nsUnicodeToISO88592::CreateInstance,
    "Unicode",
    "ISO-8859-2"
  },
  {
    &kUnicodeToISO88593CID,
    nsUnicodeToISO88593::CreateInstance,
    "Unicode",
    "ISO-8859-3"
  },
  {
    &kUnicodeToISO88594CID,
    nsUnicodeToISO88594::CreateInstance,
    "Unicode",
    "ISO-8859-4"
  },
  {
    &kUnicodeToISO88595CID,
    nsUnicodeToISO88595::CreateInstance,
    "Unicode",
    "ISO-8859-5"
  },
  {
    &kUnicodeToISO88596CID,
    nsUnicodeToISO88596::CreateInstance,
    "Unicode",
    "ISO-8859-6"
  },
  {
    &kUnicodeToISO88596ICID,
    nsUnicodeToISO88596::CreateInstance,
    "Unicode",
    "ISO-8859-6-I"
  },
  {
    &kUnicodeToISO88596ECID,
    nsUnicodeToISO88596::CreateInstance,
    "Unicode",
    "ISO-8859-6-E"
  },
  {
    &kUnicodeToISO88597CID,
    nsUnicodeToISO88597::CreateInstance,
    "Unicode",
    "ISO-8859-7"
  },
  {
    &kUnicodeToISO88598CID,
    nsUnicodeToISO88598::CreateInstance,
    "Unicode",
    "ISO-8859-8"
  },
  {
    &kUnicodeToISO88598ICID,
    nsUnicodeToISO88598::CreateInstance,
    "Unicode",
    "ISO-8859-8-I"
  },
  {
    &kUnicodeToISO88598ECID,
    nsUnicodeToISO88598::CreateInstance,
    "Unicode",
    "ISO-8859-8-E"
  },
  {
    &kUnicodeToISO88599CID,
    nsUnicodeToISO88599::CreateInstance,
    "Unicode",
    "ISO-8859-9"
  },
  {
    &kUnicodeToISO885910CID,
    nsUnicodeToISO885910::CreateInstance,
    "Unicode",
    "ISO-8859-10"
  },
  {
    &kUnicodeToISO885913CID,
    nsUnicodeToISO885913::CreateInstance,
    "Unicode",
    "ISO-8859-13"
  },
  {
    &kUnicodeToISO885914CID,
    nsUnicodeToISO885914::CreateInstance,
    "Unicode",
    "ISO-8859-14"
  },
  {
    &kUnicodeToISO885915CID,
    nsUnicodeToISO885915::CreateInstance,
    "Unicode",
    "ISO-8859-15"
  },
  {
    &kUnicodeToISOIR111CID,
    nsUnicodeToISOIR111::CreateInstance,
    "Unicode",
    "ISO-IR-111"
  },
  {
    &kUnicodeToCP1250CID,
    nsUnicodeToCP1250::CreateInstance,
    "Unicode",
    "windows-1250"
  },
  {
    &kUnicodeToCP1251CID,
    nsUnicodeToCP1251::CreateInstance,
    "Unicode",
    "windows-1251"
  },
  {
    &kUnicodeToCP1252CID,
    nsUnicodeToCP1252::CreateInstance,
    "Unicode",
    "windows-1252"
  },
  {
    &kUnicodeToCP1253CID,
    nsUnicodeToCP1253::CreateInstance,
    "Unicode",
    "windows-1253"
  },
  {
    &kUnicodeToCP1254CID,
    nsUnicodeToCP1254::CreateInstance,
    "Unicode",
    "windows-1254"
  },
  {
    &kUnicodeToCP1255CID,
    nsUnicodeToCP1255::CreateInstance,
    "Unicode",
    "windows-1255"
  },
  {
    &kUnicodeToCP1256CID,
    nsUnicodeToCP1256::CreateInstance,
    "Unicode",
    "windows-1256"
  },
  {
    &kUnicodeToCP1257CID,
    nsUnicodeToCP1257::CreateInstance,
    "Unicode",
    "windows-1257"
  },
  {
    &kUnicodeToCP1258CID,
    nsUnicodeToCP1258::CreateInstance,
    "Unicode",
    "windows-1258"
  },
  {
    &kUnicodeToCP874CID,
    nsUnicodeToCP874::CreateInstance,
    "Unicode",
    "TIS-620"
  },
  {
    &kUnicodeToCP866CID,
    nsUnicodeToCP866::CreateInstance,
    "Unicode",
    "IBM866"
  },
  {
    &kUnicodeToKOI8RCID,
    nsUnicodeToKOI8R::CreateInstance,
    "Unicode",
    "KOI8-R"
  },
  {
    &kUnicodeToKOI8UCID,
    nsUnicodeToKOI8U::CreateInstance,
    "Unicode",
    "KOI8-U"
  },
  {
    &kUnicodeToMacRomanCID,
    nsUnicodeToMacRoman::CreateInstance,
    "Unicode",
    "x-mac-roman"
  },
  {
    &kUnicodeToMacCECID,
    nsUnicodeToMacCE::CreateInstance,
    "Unicode",
    "x-mac-ce"
  },
  {
    &kUnicodeToMacGreekCID,
    nsUnicodeToMacGreek::CreateInstance,
    "Unicode",
    "x-mac-greek"
  },
  {
    &kUnicodeToMacTurkishCID,
    nsUnicodeToMacTurkish::CreateInstance,
    "Unicode",
    "x-mac-turkish"
  },
  {
    &kUnicodeToMacCroatianCID,
    nsUnicodeToMacCroatian::CreateInstance,
    "Unicode",
    "x-mac-croatian"
  },
  {
    &kUnicodeToMacRomanianCID,
    nsUnicodeToMacRomanian::CreateInstance,
    "Unicode",
    "x-mac-romanian"
  },
  {
    &kUnicodeToMacCyrillicCID,
    nsUnicodeToMacCyrillic::CreateInstance,
    "Unicode",
    "x-mac-cyrillic"
  },
  {
    &kUnicodeToMacUkrainianCID,
    nsUnicodeToMacUkrainian::CreateInstance,
    "Unicode",
    "x-mac-ukrainian"
  },
  {
    &kUnicodeToMacIcelandicCID,
    nsUnicodeToMacIcelandic::CreateInstance,
    "Unicode",
    "x-mac-icelandic"
  },
  {
    &kUnicodeToARMSCII8CID,
    nsUnicodeToARMSCII8::CreateInstance,
    "Unicode",
    "armscii-8"
  },
  {
    &kUnicodeToTCVN5712CID,
    nsUnicodeToTCVN5712::CreateInstance,
    "Unicode",
    "x-viet-tcvn5712"
  },
  {
    &kUnicodeToVISCIICID,
    nsUnicodeToVISCII::CreateInstance,
    "Unicode",
    "VISCII"
  },
  {
    &kUnicodeToVPSCID,
    nsUnicodeToVPS::CreateInstance,
    "Unicode",
    "x-viet-vps"
  },
  {
    &kUnicodeToUTF8CID,
    nsUnicodeToUTF8::CreateInstance,
    "Unicode",
    "UTF-8"
  },
  {
    &kUnicodeToUTF7CID,
    nsUnicodeToUTF7::CreateInstance,
    "Unicode",
    "UTF-7"
  },
  {
    &kUnicodeToMUTF7CID,
    nsUnicodeToMUTF7::CreateInstance,
    "Unicode",
    "x-imap4-modified-utf7"
  },
  {
    &kUnicodeToUTF16BECID,
    // nsUnicodeToUCS2BE::CreateInstance,
    NEW_UnicodeToUTF16BE,
    "Unicode",
    "UTF-16BE"
  },
  {
    &kUnicodeToUTF16LECID,
    // nsUnicodeToUCS2LE::CreateInstance,
    NEW_UnicodeToUTF16LE,
    "Unicode",
    "UTF-16LE"
  },
  {
    &kUnicodeToUTF16CID,
    NEW_UnicodeToUTF16,
    "Unicode",
    "UTF-16"
  },
  {
    &kUnicodeToUTF32BECID,
    nsUnicodeToUCS4BE::CreateInstance,
    "Unicode",
    "UTF-32BE"
  },
  {
    &kUnicodeToUTF32LECID,
    nsUnicodeToUCS4LE::CreateInstance,
    "Unicode",
    "UTF-32LE"
  },
  {
    &kUnicodeToT61CID,
    nsUnicodeToT61::CreateInstance,
    "Unicode",
    "T.61-8bit"
  },
  {
    &kUnicodeToUserDefinedCID,
    nsUnicodeToUserDefined::CreateInstance,
    "Unicode",
    "x-user-defined"
  },
  {
    &kUnicodeToSymbolCID,
    nsUnicodeToSymbol::CreateInstance,
    "Unicode",
    "Adobe-Symbol-Encoding"
  },
  {
    &kUnicodeToZapfDingbatsCID,
    nsUnicodeToZapfDingbat::CreateInstance,
    "Unicode",
    "x-zapf-dingbats"
  }
};

#define ARRAY_SIZE(_array)                                      \
     (sizeof(_array) / sizeof(_array[0]))

//----------------------------------------------------------------------------
// Class nsConverterFactory [declaration]

/**
 * General factory class for converter objects.
 * 
 * @created         24/Feb/1998
 * @author  Catalin Rotaru [CATA]
 */
class nsConverterFactory : public nsIFactory
{
  NS_DECL_ISUPPORTS

private:

  FactoryData * mData;

public:

  /**
   * Class constructor.
   */
  nsConverterFactory(FactoryData * aData);

  /**
   * Class destructor.
   */
  virtual ~nsConverterFactory();

  //--------------------------------------------------------------------------
  // Interface nsIFactory [declaration]

  NS_IMETHOD CreateInstance(nsISupports *aDelegate, const nsIID &aIID,
                            void **aResult);
  NS_IMETHOD LockFactory(PRBool aLock);
};

//----------------------------------------------------------------------------
// Class nsConverterModule [declaration]

class nsConverterModule : public nsIModule 
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMODULE

private:

  PRBool mInitialized;

  void Shutdown();

public:

  nsConverterModule();

  virtual ~nsConverterModule();

  nsresult Initialize();

};

//----------------------------------------------------------------------------
// Global functions and data [implementation]

static nsConverterModule * gModule = NULL;

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager * compMgr,
                                          nsIFile* location,
                                          nsIModule** return_cobj)
{
  nsresult rv = NS_OK;

  NS_ENSURE_ARG_POINTER(return_cobj);
  NS_ENSURE_FALSE(gModule, NS_ERROR_FAILURE);

  // Create an initialize the module instance
  nsConverterModule * m = new nsConverterModule();
  if (!m) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Increase refcnt and store away nsIModule interface to m in return_cobj
  rv = m->QueryInterface(NS_GET_IID(nsIModule), (void**)return_cobj);
  if (NS_FAILED(rv)) {
    delete m;
    m = nsnull;
  }
  gModule = m;                  // WARNING: Weak Reference
  return rv;
}

//----------------------------------------------------------------------------
// Class nsConverterFactory [implementation]

NS_IMPL_THREADSAFE_ISUPPORTS1(nsConverterFactory, nsIFactory);

nsConverterFactory::nsConverterFactory(FactoryData * aData) 
{
  mData = aData;

  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
}

nsConverterFactory::~nsConverterFactory() 
{
  PR_AtomicDecrement(&g_InstanceCount);
}

//----------------------------------------------------------------------------
// Interface nsIFactory [implementation]

NS_IMETHODIMP nsConverterFactory::CreateInstance(nsISupports *aDelegate,
                                                 const nsIID &aIID,
                                                 void **aResult)
{
  if (aResult == NULL) return NS_ERROR_NULL_POINTER;
  if (aDelegate != NULL) return NS_ERROR_NO_AGGREGATION;

  nsISupports * t;
  mData->CreateInstance(&t);
  if (t == NULL) return NS_ERROR_OUT_OF_MEMORY;
  
  NS_ADDREF(t);  // Stabilize
  
  nsresult res = t->QueryInterface(aIID, aResult);
  
  NS_RELEASE(t); // Destabilize and avoid leaks. Avoid calling delete <interface pointer>  

  return res;
}

NS_IMETHODIMP nsConverterFactory::LockFactory(PRBool aLock)
{
  if (aLock) PR_AtomicIncrement(&g_LockCount);
  else PR_AtomicDecrement(&g_LockCount);

  return NS_OK;
}

//----------------------------------------------------------------------------
// Class nsConverterModule [implementation]

NS_IMPL_THREADSAFE_ISUPPORTS1(nsConverterModule, nsIModule);

nsConverterModule::nsConverterModule()
: mInitialized(PR_FALSE)
{
  NS_INIT_ISUPPORTS();
}

nsConverterModule::~nsConverterModule()
{
  Shutdown();
}

nsresult nsConverterModule::Initialize()
{
  return NS_OK;
}

void nsConverterModule::Shutdown()
{
}

//----------------------------------------------------------------------------
// Interface nsIModule [implementation]

NS_IMETHODIMP nsConverterModule::GetClassObject(nsIComponentManager *aCompMgr,
                                                const nsCID& aClass,
                                                const nsIID& aIID,
                                                void ** r_classObj)
{
  nsresult rv;

  // Defensive programming: Initialize *r_classObj in case of error below
  if (!r_classObj) {
    return NS_ERROR_INVALID_POINTER;
  }
  *r_classObj = NULL;

  if (!mInitialized) {
    rv = Initialize();
    if (NS_FAILED(rv)) {
      return rv;
    }
    mInitialized = PR_TRUE;
  }

  FactoryData * data;
  nsConverterFactory * fact;

  // XXX cache these factories
  for (PRUint32 i=0; i<ARRAY_SIZE(g_FactoryData); i++) {
    data = &(g_FactoryData[i]);
    if (aClass.Equals(*(data->mCID))) {
      fact = new nsConverterFactory(data);
      if (fact == NULL) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      rv = fact->QueryInterface(aIID, (void **) r_classObj);
      if (NS_FAILED(rv)) delete fact;

      return rv;
    }
  }

  return NS_ERROR_FACTORY_NOT_REGISTERED;
}

NS_IMETHODIMP nsConverterModule::RegisterSelf(nsIComponentManager *aCompMgr,
                                              nsIFile* aPath,
                                              const char* registryLocation,
                                              const char* componentType)
{
  nsresult res;
  PRUint32 i;
  nsIRegistry * registry = NULL;
  nsRegistryKey key;
  char buff[1024];

  // get the registry
  res = nsServiceManager::GetService(NS_REGISTRY_CONTRACTID, 
      NS_GET_IID(nsIRegistry), (nsISupports**)&registry);
  if (NS_FAILED(res)) goto done;

  // open the registry
  res = registry->OpenWellKnownRegistry(
      nsIRegistry::ApplicationComponentRegistry);
  if (NS_FAILED(res)) goto done;

  char name[128];
  char contractid[128];
  char * cid_string;
  for (i=0; i<ARRAY_SIZE(g_FactoryData); i++) {
    if(0==PL_strcmp(g_FactoryData[i].mCharsetSrc,"Unicode"))
    {
       PL_strcpy(name, ENCODER_NAME_BASE);
       PL_strcat(name, g_FactoryData[i].mCharsetDest);
       PL_strcpy(contractid, NS_UNICODEENCODER_CONTRACTID_BASE);
       PL_strcat(contractid, g_FactoryData[i].mCharsetDest);
    } else {
       PL_strcpy(name, DECODER_NAME_BASE);
       PL_strcat(name, g_FactoryData[i].mCharsetSrc);
       PL_strcpy(contractid, NS_UNICODEDECODER_CONTRACTID_BASE);
       PL_strcat(contractid, g_FactoryData[i].mCharsetSrc);
    }
    // register component
    res = aCompMgr->RegisterComponentSpec(*(g_FactoryData[i].mCID), name, 
      contractid, aPath, PR_TRUE, PR_TRUE);
    if(NS_FAILED(res) && (NS_ERROR_FACTORY_EXISTS != res)) goto done;

    // register component info
    // XXX take these KONSTANTS out of here; refine this code
    cid_string = g_FactoryData[i].mCID->ToString();
    sprintf(buff, "%s/%s", "software/netscape/intl/uconv", cid_string);
    nsCRT::free(cid_string);
    res = registry -> AddSubtree(nsIRegistry::Common, buff, &key);
    if (NS_FAILED(res)) goto done;
    res = registry -> SetStringUTF8(key, "source", g_FactoryData[i].mCharsetSrc);
    if (NS_FAILED(res)) goto done;
    res = registry -> SetStringUTF8(key, "destination", g_FactoryData[i].mCharsetDest);
    if (NS_FAILED(res)) goto done;
  }

done:
  if (registry != NULL) {
    nsServiceManager::ReleaseService(NS_REGISTRY_CONTRACTID, registry);
  }

  return res;
}

NS_IMETHODIMP nsConverterModule::UnregisterSelf(nsIComponentManager *aCompMgr,
                                                nsIFile* aPath,
                                                const char* registryLocation)
{
  // XXX also delete the stuff I added to the registry
  nsresult rv;

  for (PRUint32 i=0; i<ARRAY_SIZE(g_FactoryData); i++) {
    rv = aCompMgr->UnregisterComponentSpec(*(g_FactoryData[i].mCID), aPath);
  }

  return NS_OK;
}

NS_IMETHODIMP nsConverterModule::CanUnload(nsIComponentManager *aCompMgr, 
                                           PRBool *okToUnload)
{
  if (!okToUnload) {
    return NS_ERROR_INVALID_POINTER;
  }
  *okToUnload = (g_InstanceCount == 0 && g_LockCount == 0);
  return NS_OK;
}

