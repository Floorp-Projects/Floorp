/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   IBM Corporation
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
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsIComponentManager.h"
#include "nsICategoryManager.h"
#include "nsICharsetConverterManager.h"
#include "nsEncoderDecoderUtils.h"
#include "nsIUnicodeDecoder.h"
#include "nsIUnicodeEncoder.h"
#include "nsICharsetAlias.h"
#include "nsIServiceManager.h"


#include "nsUConvCID.h"
#include "nsCharsetConverterManager.h"
#include "nsCharsetAlias.h"
#include "nsTextToSubURI.h"
#include "nsUTF8ConverterService.h"
#include "nsConverterInputStream.h"
#include "nsConverterOutputStream.h"
#include "nsPlatformCharset.h"
#include "nsScriptableUConv.h"

#ifndef MOZ_USE_NATIVE_UCONV
#include "nsIPlatformCharset.h"
#include "nsITextToSubURI.h"

#include "nsUConvDll.h"
#include "nsIFile.h"

#include "nsCRT.h"

#include "nsUCSupport.h"
#include "nsISO88591ToUnicode.h"
#include "nsCP1252ToUnicode.h"
#include "nsMacRomanToUnicode.h"
#include "nsUTF8ToUnicode.h"
#include "nsUnicodeToISO88591.h"
#include "nsUnicodeToCP1252.h"
#include "nsUnicodeToMacRoman.h"
#include "nsUnicodeToUTF8.h"

// ucvlatin
#include "nsUCvLatinCID.h"
#include "nsUCvLatinDll.h"
#include "nsAsciiToUnicode.h"
#include "nsISO88592ToUnicode.h"
#include "nsISO88593ToUnicode.h"
#include "nsISO88594ToUnicode.h"
#include "nsISO88595ToUnicode.h"
#include "nsISO88596ToUnicode.h"
#include "nsISO88596EToUnicode.h"
#include "nsISO88596IToUnicode.h"
#include "nsISO88597ToUnicode.h"
#include "nsISO88598ToUnicode.h"
#include "nsISO88598EToUnicode.h"
#include "nsISO88598IToUnicode.h"
#include "nsISO88599ToUnicode.h"
#include "nsISO885910ToUnicode.h"
#include "nsISO885913ToUnicode.h"
#include "nsISO885914ToUnicode.h"
#include "nsISO885915ToUnicode.h"
#include "nsISO885916ToUnicode.h"
#include "nsISOIR111ToUnicode.h"
#include "nsCP1250ToUnicode.h"
#include "nsCP1251ToUnicode.h"
#include "nsCP1253ToUnicode.h"
#include "nsCP1254ToUnicode.h"
#include "nsCP1255ToUnicode.h"
#include "nsCP1256ToUnicode.h"
#include "nsCP1257ToUnicode.h"
#include "nsCP1258ToUnicode.h"
#include "nsCP874ToUnicode.h"
#include "nsISO885911ToUnicode.h"
#include "nsTIS620ToUnicode.h"
#include "nsCP866ToUnicode.h"
#include "nsKOI8RToUnicode.h"
#include "nsKOI8UToUnicode.h"
#include "nsMacCEToUnicode.h"
#include "nsMacGreekToUnicode.h"
#include "nsMacTurkishToUnicode.h"
#include "nsMacCroatianToUnicode.h"
#include "nsMacRomanianToUnicode.h"
#include "nsMacCyrillicToUnicode.h"
#include "nsMacUkrainianToUnicode.h"
#include "nsMacIcelandicToUnicode.h"
#include "nsGEOSTD8ToUnicode.h"
#include "nsARMSCII8ToUnicode.h"
#include "nsTCVN5712ToUnicode.h"
#include "nsVISCIIToUnicode.h"
#include "nsVPSToUnicode.h"
#include "nsUTF7ToUnicode.h"
#include "nsMUTF7ToUnicode.h"
#include "nsUTF32ToUnicode.h"
#include "nsUCS2BEToUnicode.h"
#include "nsT61ToUnicode.h"
#include "nsUserDefinedToUnicode.h"
#include "nsUnicodeToAscii.h"
#include "nsUnicodeToISO88592.h"
#include "nsUnicodeToISO88593.h"
#include "nsUnicodeToISO88594.h"
#include "nsUnicodeToISO88595.h"
#include "nsUnicodeToISO88596.h"
#include "nsUnicodeToISO88596E.h"
#include "nsUnicodeToISO88596I.h"
#include "nsUnicodeToISO88597.h"
#include "nsUnicodeToISO88598.h"
#include "nsUnicodeToISO88598E.h"
#include "nsUnicodeToISO88598I.h"
#include "nsUnicodeToISO88599.h"
#include "nsUnicodeToISO885910.h"
#include "nsUnicodeToISO885913.h"
#include "nsUnicodeToISO885914.h"
#include "nsUnicodeToISO885915.h"
#include "nsUnicodeToISO885916.h"
#include "nsUnicodeToISOIR111.h"
#include "nsUnicodeToCP1250.h"
#include "nsUnicodeToCP1251.h"
#include "nsUnicodeToCP1253.h"
#include "nsUnicodeToCP1254.h"
#include "nsUnicodeToCP1255.h"
#include "nsUnicodeToCP1256.h"
#include "nsUnicodeToCP1257.h"
#include "nsUnicodeToCP1258.h"
#include "nsUnicodeToCP874.h"
#include "nsUnicodeToISO885911.h"
#include "nsUnicodeToTIS620.h"
#include "nsUnicodeToCP866.h"
#include "nsUnicodeToKOI8R.h"
#include "nsUnicodeToKOI8U.h"
#include "nsUnicodeToMacCE.h"
#include "nsUnicodeToMacGreek.h"
#include "nsUnicodeToMacTurkish.h"
#include "nsUnicodeToMacCroatian.h"
#include "nsUnicodeToMacRomanian.h"
#include "nsUnicodeToMacCyrillic.h"
#include "nsUnicodeToMacUkrainian.h"
#include "nsUnicodeToMacIcelandic.h"
#include "nsUnicodeToGEOSTD8.h"
#include "nsUnicodeToARMSCII8.h"
#include "nsUnicodeToTCVN5712.h"
#include "nsUnicodeToVISCII.h"
#include "nsUnicodeToVPS.h"
#include "nsUnicodeToUTF7.h"
#include "nsUnicodeToMUTF7.h"
#include "nsUnicodeToUCS2BE.h"
#include "nsUnicodeToUTF32.h"
#include "nsUnicodeToT61.h"
#include "nsUnicodeToUserDefined.h"
#include "nsUnicodeToSymbol.h"
#include "nsUnicodeToZapfDingbat.h"
#include "nsUnicodeToAdobeEuro.h"
#include "nsMacArabicToUnicode.h"
#include "nsMacDevanagariToUnicode.h"
#include "nsMacFarsiToUnicode.h"
#include "nsMacGujaratiToUnicode.h"
#include "nsMacGurmukhiToUnicode.h"
#include "nsMacHebrewToUnicode.h"
#include "nsUnicodeToMacArabic.h"
#include "nsUnicodeToMacDevanagari.h"
#include "nsUnicodeToMacFarsi.h"
#include "nsUnicodeToMacGujarati.h"
#include "nsUnicodeToMacGurmukhi.h"
#include "nsUnicodeToMacHebrew.h"
#include "nsUnicodeToTSCII.h"
#ifdef MOZ_EXTRA_X11CONVERTERS
#include "nsUnicodeToLangBoxArabic8.h"
#include "nsUnicodeToLangBoxArabic16.h"
#endif // MOZ_EXTRA_X11CONVERTERS

// ucvibm
#include "nsUCvIBMCID.h"
#include "nsUCvIBMDll.h"
#include "nsCP850ToUnicode.h"
#include "nsCP852ToUnicode.h"
#include "nsCP855ToUnicode.h"
#include "nsCP857ToUnicode.h"
#include "nsCP862ToUnicode.h"
#include "nsCP864ToUnicode.h"
#include "nsCP864iToUnicode.h"
#ifdef MOZ_EXTRA_X11CONVERTERS
#include "nsCP1046ToUnicode.h"
#endif
#ifdef XP_OS2
#include "nsCP869ToUnicode.h"
#include "nsCP1125ToUnicode.h"
#include "nsCP1131ToUnicode.h"
#endif
#include "nsUnicodeToCP850.h"
#include "nsUnicodeToCP852.h"
#include "nsUnicodeToCP855.h"
#include "nsUnicodeToCP857.h"
#include "nsUnicodeToCP862.h"
#include "nsUnicodeToCP864.h"
#include "nsUnicodeToCP864i.h"
#ifdef MOZ_EXTRA_X11CONVERTERS
#include "nsUnicodeToCP1046.h"
#endif
#ifdef XP_OS2
#include "nsUnicodeToCP869.h"
#include "nsUnicodeToCP1125.h"
#include "nsUnicodeToCP1131.h"
#endif

// ucvja
#include "nsUCVJACID.h"
#include "nsUCVJA2CID.h"
#include "nsUCVJADll.h"
#include "nsJapaneseToUnicode.h"
#include "nsUnicodeToSJIS.h"
#include "nsUnicodeToEUCJP.h"
#include "nsUnicodeToISO2022JP.h"
#include "nsUnicodeToJISx0201.h"
#ifdef MOZ_EXTRA_X11CONVERTERS
#include "nsUnicodeToJISx0208.h"
#include "nsUnicodeToJISx0212.h"
#endif

// ucvtw2
#include "nsUCvTW2CID.h"
#include "nsUCvTW2Dll.h"
#include "nsEUCTWToUnicode.h"
#include "nsUnicodeToEUCTW.h"
#ifdef MOZ_EXTRA_X11CONVERTERS
#include "nsUnicodeToCNS11643p1.h"
#include "nsUnicodeToCNS11643p2.h"
#include "nsUnicodeToCNS11643p3.h"
#include "nsUnicodeToCNS11643p4.h"
#include "nsUnicodeToCNS11643p5.h"
#include "nsUnicodeToCNS11643p6.h"
#include "nsUnicodeToCNS11643p7.h"
#endif

// ucvtw
#include "nsUCvTWCID.h"
#include "nsUCvTWDll.h"
#include "nsBIG5ToUnicode.h"
#include "nsUnicodeToBIG5.h"
#ifdef MOZ_EXTRA_X11CONVERTERS
#include "nsUnicodeToBIG5NoAscii.h"
#endif
#include "nsBIG5HKSCSToUnicode.h"
#include "nsUnicodeToBIG5HKSCS.h"
#include "nsUnicodeToHKSCS.h"

// ucvko
#include "nsUCvKOCID.h"
#include "nsUCvKODll.h"
#include "nsEUCKRToUnicode.h"
#include "nsUnicodeToEUCKR.h"
#include "nsJohabToUnicode.h"
#include "nsUnicodeToJohab.h"
#ifdef MOZ_EXTRA_X11CONVERTERS
#include "nsUnicodeToKSC5601.h"
#include "nsUnicodeToX11Johab.h"
#include "nsUnicodeToJohabNoAscii.h"
#endif
#include "nsCP949ToUnicode.h"
#include "nsUnicodeToCP949.h"
#include "nsISO2022KRToUnicode.h"
#include "nsUnicodeToJamoTTF.h"

// ucvcn
#include "nsUCvCnCID.h"
#include "nsUCvCnDll.h"
#include "nsHZToUnicode.h"
#include "nsUnicodeToHZ.h"
#include "nsGBKToUnicode.h"
#include "nsUnicodeToGBK.h"
#ifdef MOZ_EXTRA_X11CONVERTERS
#include "nsUnicodeToGBKNoAscii.h"
#endif
#include "nsCP936ToUnicode.h"
#include "nsUnicodeToCP936.h"
#include "nsGB2312ToUnicodeV2.h"
#include "nsUnicodeToGB2312V2.h"
#ifdef MOZ_EXTRA_X11CONVERTERS
#include "nsUnicodeToGB2312GL.h"
#endif
#include "nsISO2022CNToUnicode.h"
#include "nsUnicodeToISO2022CN.h"
#include "gbku.h"

#define DECODER_NAME_BASE "Unicode Decoder-"
#define ENCODER_NAME_BASE "Unicode Encoder-"

NS_CONVERTER_REGISTRY_START
NS_UCONV_REG_UNREG("ISO-8859-1", NS_ISO88591TOUNICODE_CID, NS_UNICODETOISO88591_CID)
NS_UCONV_REG_UNREG("windows-1252", NS_CP1252TOUNICODE_CID, NS_UNICODETOCP1252_CID)
NS_UCONV_REG_UNREG("x-mac-roman", NS_MACROMANTOUNICODE_CID, NS_UNICODETOMACROMAN_CID)
NS_UCONV_REG_UNREG("UTF-8", NS_UTF8TOUNICODE_CID, NS_UNICODETOUTF8_CID)

  // ucvlatin
NS_UCONV_REG_UNREG("us-ascii", NS_ASCIITOUNICODE_CID, NS_UNICODETOASCII_CID)
NS_UCONV_REG_UNREG("ISO-8859-2", NS_ISO88592TOUNICODE_CID, NS_UNICODETOISO88592_CID)
NS_UCONV_REG_UNREG("ISO-8859-3", NS_ISO88593TOUNICODE_CID, NS_UNICODETOISO88593_CID)
NS_UCONV_REG_UNREG("ISO-8859-4", NS_ISO88594TOUNICODE_CID, NS_UNICODETOISO88594_CID)
NS_UCONV_REG_UNREG("ISO-8859-5", NS_ISO88595TOUNICODE_CID, NS_UNICODETOISO88595_CID)
NS_UCONV_REG_UNREG("ISO-8859-6", NS_ISO88596TOUNICODE_CID, NS_UNICODETOISO88596_CID)
NS_UCONV_REG_UNREG("ISO-8859-6-I", NS_ISO88596ITOUNICODE_CID, NS_UNICODETOISO88596I_CID)
NS_UCONV_REG_UNREG("ISO-8859-6-E", NS_ISO88596ETOUNICODE_CID, NS_UNICODETOISO88596E_CID)
NS_UCONV_REG_UNREG("ISO-8859-7", NS_ISO88597TOUNICODE_CID, NS_UNICODETOISO88597_CID)
NS_UCONV_REG_UNREG("ISO-8859-8", NS_ISO88598TOUNICODE_CID, NS_UNICODETOISO88598_CID)
NS_UCONV_REG_UNREG("ISO-8859-8-I", NS_ISO88598ITOUNICODE_CID, NS_UNICODETOISO88598I_CID)
NS_UCONV_REG_UNREG("ISO-8859-8-E", NS_ISO88598ETOUNICODE_CID, NS_UNICODETOISO88598E_CID)
NS_UCONV_REG_UNREG("ISO-8859-9", NS_ISO88599TOUNICODE_CID, NS_UNICODETOISO88599_CID)
NS_UCONV_REG_UNREG("ISO-8859-10", NS_ISO885910TOUNICODE_CID, NS_UNICODETOISO885910_CID)
NS_UCONV_REG_UNREG("ISO-8859-13", NS_ISO885913TOUNICODE_CID, NS_UNICODETOISO885913_CID)
NS_UCONV_REG_UNREG("ISO-8859-14", NS_ISO885914TOUNICODE_CID, NS_UNICODETOISO885914_CID)
NS_UCONV_REG_UNREG("ISO-8859-15", NS_ISO885915TOUNICODE_CID, NS_UNICODETOISO885915_CID)
NS_UCONV_REG_UNREG("ISO-8859-16", NS_ISO885916TOUNICODE_CID, NS_UNICODETOISO885916_CID)
NS_UCONV_REG_UNREG("ISO-IR-111", NS_ISOIR111TOUNICODE_CID, NS_UNICODETOISOIR111_CID)
NS_UCONV_REG_UNREG("windows-1250", NS_CP1250TOUNICODE_CID, NS_UNICODETOCP1250_CID)
NS_UCONV_REG_UNREG("windows-1251", NS_CP1251TOUNICODE_CID, NS_UNICODETOCP1251_CID)
NS_UCONV_REG_UNREG("windows-1253", NS_CP1253TOUNICODE_CID, NS_UNICODETOCP1253_CID)
NS_UCONV_REG_UNREG("windows-1254", NS_CP1254TOUNICODE_CID, NS_UNICODETOCP1254_CID)
NS_UCONV_REG_UNREG("windows-1255", NS_CP1255TOUNICODE_CID, NS_UNICODETOCP1255_CID)
NS_UCONV_REG_UNREG("windows-1256", NS_CP1256TOUNICODE_CID, NS_UNICODETOCP1256_CID)
NS_UCONV_REG_UNREG("windows-1257", NS_CP1257TOUNICODE_CID, NS_UNICODETOCP1257_CID)
NS_UCONV_REG_UNREG("windows-1258", NS_CP1258TOUNICODE_CID, NS_UNICODETOCP1258_CID)
NS_UCONV_REG_UNREG("TIS-620", NS_TIS620TOUNICODE_CID, NS_UNICODETOTIS620_CID)
NS_UCONV_REG_UNREG("windows-874", NS_CP874TOUNICODE_CID, NS_UNICODETOCP874_CID)
NS_UCONV_REG_UNREG("ISO-8859-11", NS_ISO885911TOUNICODE_CID, NS_UNICODETOISO885911_CID)
NS_UCONV_REG_UNREG("IBM866", NS_CP866TOUNICODE_CID, NS_UNICODETOCP866_CID)
NS_UCONV_REG_UNREG("KOI8-R", NS_KOI8RTOUNICODE_CID, NS_UNICODETOKOI8R_CID)
NS_UCONV_REG_UNREG("KOI8-U", NS_KOI8UTOUNICODE_CID, NS_UNICODETOKOI8U_CID)
NS_UCONV_REG_UNREG("x-mac-ce", NS_MACCETOUNICODE_CID, NS_UNICODETOMACCE_CID)
NS_UCONV_REG_UNREG("x-mac-greek", NS_MACGREEKTOUNICODE_CID, NS_UNICODETOMACGREEK_CID)
NS_UCONV_REG_UNREG("x-mac-turkish", NS_MACTURKISHTOUNICODE_CID, NS_UNICODETOMACTURKISH_CID)
NS_UCONV_REG_UNREG("x-mac-croatian", NS_MACCROATIANTOUNICODE_CID, NS_UNICODETOMACCROATIAN_CID)
NS_UCONV_REG_UNREG("x-mac-romanian", NS_MACROMANIANTOUNICODE_CID, NS_UNICODETOMACROMANIAN_CID)
NS_UCONV_REG_UNREG("x-mac-cyrillic", NS_MACCYRILLICTOUNICODE_CID, NS_UNICODETOMACCYRILLIC_CID)
NS_UCONV_REG_UNREG("x-mac-ukrainian", NS_MACUKRAINIANTOUNICODE_CID, NS_UNICODETOMACUKRAINIAN_CID)
NS_UCONV_REG_UNREG("x-mac-icelandic", NS_MACICELANDICTOUNICODE_CID, NS_UNICODETOMACICELANDIC_CID)
NS_UCONV_REG_UNREG("GEOSTD8", NS_GEOSTD8TOUNICODE_CID, NS_UNICODETOGEOSTD8_CID)
NS_UCONV_REG_UNREG("armscii-8", NS_ARMSCII8TOUNICODE_CID, NS_UNICODETOARMSCII8_CID)
NS_UCONV_REG_UNREG("x-viet-tcvn5712", NS_TCVN5712TOUNICODE_CID, NS_UNICODETOTCVN5712_CID)
NS_UCONV_REG_UNREG("VISCII", NS_VISCIITOUNICODE_CID, NS_UNICODETOVISCII_CID)
NS_UCONV_REG_UNREG("x-viet-vps", NS_VPSTOUNICODE_CID, NS_UNICODETOVPS_CID)
NS_UCONV_REG_UNREG("UTF-7", NS_UTF7TOUNICODE_CID, NS_UNICODETOUTF7_CID)
NS_UCONV_REG_UNREG("x-imap4-modified-utf7", NS_MUTF7TOUNICODE_CID, NS_UNICODETOMUTF7_CID)
NS_UCONV_REG_UNREG("UTF-16", NS_UTF16TOUNICODE_CID, NS_UNICODETOUTF16_CID)
NS_UCONV_REG_UNREG("UTF-16BE", NS_UTF16BETOUNICODE_CID, NS_UNICODETOUTF16BE_CID)
NS_UCONV_REG_UNREG("UTF-16LE", NS_UTF16LETOUNICODE_CID, NS_UNICODETOUTF16LE_CID)
NS_UCONV_REG_UNREG("UTF-32BE", NS_UTF32BETOUNICODE_CID, NS_UNICODETOUTF32BE_CID)
NS_UCONV_REG_UNREG("UTF-32LE", NS_UTF32LETOUNICODE_CID, NS_UNICODETOUTF32LE_CID)
NS_UCONV_REG_UNREG("T.61-8bit", NS_T61TOUNICODE_CID, NS_UNICODETOT61_CID)
NS_UCONV_REG_UNREG("x-user-defined", NS_USERDEFINEDTOUNICODE_CID, NS_UNICODETOUSERDEFINED_CID)
NS_UCONV_REG_UNREG("x-mac-arabic" , NS_MACARABICTOUNICODE_CID, NS_UNICODETOMACARABIC_CID)
NS_UCONV_REG_UNREG("x-mac-devanagari" , NS_MACDEVANAGARITOUNICODE_CID, NS_UNICODETOMACDEVANAGARI_CID)
NS_UCONV_REG_UNREG("x-mac-farsi" , NS_MACFARSITOUNICODE_CID, NS_UNICODETOMACFARSI_CID)
NS_UCONV_REG_UNREG("x-mac-gurmukhi" , NS_MACGURMUKHITOUNICODE_CID, NS_UNICODETOMACGURMUKHI_CID)
NS_UCONV_REG_UNREG("x-mac-gujarati" , NS_MACGUJARATITOUNICODE_CID, NS_UNICODETOMACGUJARATI_CID)
NS_UCONV_REG_UNREG("x-mac-hebrew" , NS_MACHEBREWTOUNICODE_CID, NS_UNICODETOMACHEBREW_CID)

NS_UCONV_REG_UNREG_ENCODER("Adobe-Symbol-Encoding" , NS_UNICODETOSYMBOL_CID)
NS_UCONV_REG_UNREG_ENCODER("x-zapf-dingbats" , NS_UNICODETOZAPFDINGBATS_CID)
NS_UCONV_REG_UNREG_ENCODER("x-tscii",  NS_UNICODETOTSCII_CID)
NS_UCONV_REG_UNREG_ENCODER("x-tamilttf-0",  NS_UNICODETOTAMILTTF_CID)
#ifdef MOZ_EXTRA_X11CONVERTERS
NS_UCONV_REG_UNREG_ENCODER("x-iso-8859-6-8-x" , NS_UNICODETOLANGBOXARABIC_CID)
NS_UCONV_REG_UNREG_ENCODER("x-iso-8859-6-16" , NS_UNICODETOLANGBOXARABIC16_CID)
#endif // MOZ_EXTRA_X11CONVERTERS

  // ucvibm
NS_UCONV_REG_UNREG("IBM850", NS_CP850TOUNICODE_CID, NS_UNICODETOCP850_CID)
NS_UCONV_REG_UNREG("IBM852", NS_CP852TOUNICODE_CID, NS_UNICODETOCP852_CID)
NS_UCONV_REG_UNREG("IBM855", NS_CP855TOUNICODE_CID, NS_UNICODETOCP855_CID)
NS_UCONV_REG_UNREG("IBM857", NS_CP857TOUNICODE_CID, NS_UNICODETOCP857_CID)
NS_UCONV_REG_UNREG("IBM862", NS_CP862TOUNICODE_CID, NS_UNICODETOCP862_CID)
NS_UCONV_REG_UNREG("IBM864", NS_CP864TOUNICODE_CID, NS_UNICODETOCP864_CID)
NS_UCONV_REG_UNREG("IBM864i", NS_CP864ITOUNICODE_CID, NS_UNICODETOCP864I_CID)
#ifdef MOZ_EXTRA_X11CONVERTERS
NS_UCONV_REG_UNREG("x-IBM1046", NS_CP1046TOUNICODE_CID, NS_UNICODETOCP1046_CID)
#endif
#ifdef XP_OS2
NS_UCONV_REG_UNREG("IBM869", NS_CP869TOUNICODE_CID, NS_UNICODETOCP869_CID)
NS_UCONV_REG_UNREG("IBM1125", NS_CP1125TOUNICODE_CID, NS_UNICODETOCP1125_CID)
NS_UCONV_REG_UNREG("IBM1131", NS_CP1131TOUNICODE_CID, NS_UNICODETOCP1131_CID)
#endif

    // ucvja
NS_UCONV_REG_UNREG("Shift_JIS", NS_SJISTOUNICODE_CID, NS_UNICODETOSJIS_CID)
NS_UCONV_REG_UNREG("ISO-2022-JP", NS_ISO2022JPTOUNICODE_CID, NS_UNICODETOISO2022JP_CID)
NS_UCONV_REG_UNREG("EUC-JP", NS_EUCJPTOUNICODE_CID, NS_UNICODETOEUCJP_CID)
  
NS_UCONV_REG_UNREG_ENCODER("jis_0201" , NS_UNICODETOJISX0201_CID)
#ifdef MOZ_EXTRA_X11CONVERTERS
NS_UCONV_REG_UNREG_ENCODER("jis_0208-1983" , NS_UNICODETOJISX0208_CID)
NS_UCONV_REG_UNREG_ENCODER("jis_0212-1990" , NS_UNICODETOJISX0212_CID)
#endif

    // ucvtw2
NS_UCONV_REG_UNREG("x-euc-tw", NS_EUCTWTOUNICODE_CID, NS_UNICODETOEUCTW_CID)
#ifdef MOZ_EXTRA_X11CONVERTERS
NS_UCONV_REG_UNREG_ENCODER("x-cns-11643-1" , NS_UNICODETOCNS11643P1_CID)
NS_UCONV_REG_UNREG_ENCODER("x-cns-11643-2" , NS_UNICODETOCNS11643P2_CID)
NS_UCONV_REG_UNREG_ENCODER("x-cns-11643-3" , NS_UNICODETOCNS11643P3_CID)
NS_UCONV_REG_UNREG_ENCODER("x-cns-11643-4" , NS_UNICODETOCNS11643P4_CID)
NS_UCONV_REG_UNREG_ENCODER("x-cns-11643-5" , NS_UNICODETOCNS11643P5_CID)
NS_UCONV_REG_UNREG_ENCODER("x-cns-11643-6" , NS_UNICODETOCNS11643P6_CID)
NS_UCONV_REG_UNREG_ENCODER("x-cns-11643-7" , NS_UNICODETOCNS11643P7_CID)
#endif

    // ucvtw
NS_UCONV_REG_UNREG("Big5", NS_BIG5TOUNICODE_CID, NS_UNICODETOBIG5_CID)
NS_UCONV_REG_UNREG("Big5-HKSCS", NS_BIG5HKSCSTOUNICODE_CID, NS_UNICODETOBIG5HKSCS_CID)
  
NS_UCONV_REG_UNREG_ENCODER("hkscs-1" , NS_UNICODETOHKSCS_CID)
#ifdef MOZ_EXTRA_X11CONVERTERS
NS_UCONV_REG_UNREG_ENCODER("x-x-big5",  NS_UNICODETOBIG5NOASCII_CID)
#endif

    // ucvko
NS_UCONV_REG_UNREG("EUC-KR", NS_EUCKRTOUNICODE_CID, NS_UNICODETOEUCKR_CID)
NS_UCONV_REG_UNREG("x-johab", NS_JOHABTOUNICODE_CID, NS_UNICODETOJOHAB_CID)
NS_UCONV_REG_UNREG("x-windows-949", NS_CP949TOUNICODE_CID, NS_UNICODETOCP949_CID)
NS_UCONV_REG_UNREG_DECODER("ISO-2022-KR", NS_ISO2022KRTOUNICODE_CID)
NS_UCONV_REG_UNREG_ENCODER("x-koreanjamo-0",  NS_UNICODETOJAMOTTF_CID)

#ifdef MOZ_EXTRA_X11CONVERTERS
NS_UCONV_REG_UNREG_ENCODER("ks_c_5601-1987",  NS_UNICODETOKSC5601_CID)
NS_UCONV_REG_UNREG_ENCODER("x-x11johab",  NS_UNICODETOX11JOHAB_CID)
NS_UCONV_REG_UNREG_ENCODER("x-johab-noascii",  NS_UNICODETOJOHABNOASCII_CID)
#endif

// ucvcn
NS_UCONV_REG_UNREG("GB2312", NS_GB2312TOUNICODE_CID, NS_UNICODETOGB2312_CID)
NS_UCONV_REG_UNREG("windows-936", NS_CP936TOUNICODE_CID, NS_UNICODETOCP936_CID)
NS_UCONV_REG_UNREG("x-gbk", NS_GBKTOUNICODE_CID, NS_UNICODETOGBK_CID)
#ifdef MOZ_EXTRA_X11CONVERTERS
NS_UCONV_REG_UNREG_ENCODER("x-gbk-noascii",  NS_UNICODETOGBKNOASCII_CID)
#endif
NS_UCONV_REG_UNREG("HZ-GB-2312", NS_HZTOUNICODE_CID, NS_UNICODETOHZ_CID)
#ifdef MOZ_EXTRA_X11CONVERTERS
NS_UCONV_REG_UNREG_ENCODER("gb_2312-80",  NS_UNICODETOGB2312GL_CID)
#endif
NS_UCONV_REG_UNREG("gb18030", NS_GB18030TOUNICODE_CID, NS_UNICODETOGB18030_CID)
#ifdef MOZ_EXTRA_X11CONVERTERS
NS_UCONV_REG_UNREG_ENCODER("gb18030.2000-0",  NS_UNICODETOGB18030Font0_CID)
NS_UCONV_REG_UNREG_ENCODER("gb18030.2000-1",  NS_UNICODETOGB18030Font1_CID)
#endif
NS_UCONV_REG_UNREG_DECODER("ISO-2022-CN", NS_ISO2022CNTOUNICODE_CID)
  
NS_CONVERTER_REGISTRY_END

NS_IMPL_NSUCONVERTERREGSELF

NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToUTF8)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUTF8ToUnicode)

// ucvlatin
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUTF7ToUnicode)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMUTF7ToUnicode)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUTF16ToUnicode)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUTF16BEToUnicode)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUTF16LEToUnicode)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUTF32BEToUnicode)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUTF32LEToUnicode)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToUTF7)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToMUTF7)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToUTF16BE)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToUTF16LE)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToUTF16)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToUTF32BE)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToUTF32LE)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToTSCII)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToTamilTTF)
#ifdef MOZ_EXTRA_X11CONVERTERS
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToLangBoxArabic8)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToLangBoxArabic16)
#endif // MOZ_EXTRA_X11CONVERTERS

// ucvibm

// ucvja
NS_GENERIC_FACTORY_CONSTRUCTOR(nsShiftJISToUnicode)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsEUCJPToUnicodeV2)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsISO2022JPToUnicodeV2)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToISO2022JP)

// ucvtw2

// ucvtw
#ifdef MOZ_EXTRA_X11CONVERTERS
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToBIG5NoAscii)
#endif

// ucvko
#ifdef MOZ_EXTRA_X11CONVERTERS
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToX11Johab)
#endif
NS_GENERIC_FACTORY_CONSTRUCTOR(nsISO2022KRToUnicode)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToJamoTTF)

// ucvcn
NS_GENERIC_FACTORY_CONSTRUCTOR(nsGB2312ToUnicodeV2)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToGB2312V2)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCP936ToUnicode)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToCP936)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsGBKToUnicode)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToGBK)
#ifdef MOZ_EXTRA_X11CONVERTERS
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToGBKNoAscii)
#endif
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHZToUnicode)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToHZ)
#ifdef MOZ_EXTRA_X11CONVERTERS
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToGB2312GL)
#endif
NS_GENERIC_FACTORY_CONSTRUCTOR(nsGB18030ToUnicode)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToGB18030)
#ifdef MOZ_EXTRA_X11CONVERTERS
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToGB18030Font0)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToGB18030Font1)
#endif
NS_GENERIC_FACTORY_CONSTRUCTOR(nsISO2022CNToUnicode)


//----------------------------------------------------------------------------
// Global functions and data [declaration]

#define DECODER_NAME_BASE "Unicode Decoder-"
#define ENCODER_NAME_BASE "Unicode Encoder-"

// ucvja
const PRUint16 g_uf0201Mapping[] = {
#include "jis0201.uf"
};

const PRUint16 g_uf0201GLMapping[] = {
#include "jis0201gl.uf"
};

const PRUint16 g_uf0208Mapping[] = {
#include "jis0208.uf"
};

const PRUint16 g_uf0208extMapping[] = {
#include "jis0208ext.uf"
};

const PRUint16 g_uf0212Mapping[] = {
#include "jis0212.uf"
};

// ucvtw2
const PRUint16 g_ufCNS1MappingTable[] = {
#include "cns_1.uf"
};

const PRUint16 g_ufCNS2MappingTable[] = {
#include "cns_2.uf"
};

const PRUint16 g_ufCNS3MappingTable[] = {
#include "cns3.uf"
};

const PRUint16 g_ufCNS4MappingTable[] = {
#include "cns4.uf"
};

const PRUint16 g_ufCNS5MappingTable[] = {
#include "cns5.uf"
};

const PRUint16 g_ufCNS6MappingTable[] = {
#include "cns6.uf"
};

const PRUint16 g_ufCNS7MappingTable[] = {
#include "cns7.uf"
};

const PRUint16 g_utCNS1MappingTable[] = {
#include "cns_1.ut"
};

const PRUint16 g_utCNS2MappingTable[] = {
#include "cns_2.ut"
};

const PRUint16 g_utCNS3MappingTable[] = {
#include "cns3.ut"
};

const PRUint16 g_utCNS4MappingTable[] = {
#include "cns4.ut"
};

const PRUint16 g_utCNS5MappingTable[] = {
#include "cns5.ut"
};

const PRUint16 g_utCNS6MappingTable[] = {
#include "cns6.ut"
};

const PRUint16 g_utCNS7MappingTable[] = {
#include "cns7.ut"
};

const PRUint16 g_ASCIIMappingTable[] = {
  0x0001, 0x0004, 0x0005, 0x0008, 0x0000, 0x0000, 0x007F, 0x0000
};

// ucvtw
const PRUint16 g_ufBig5Mapping[] = {
#include "big5.uf"
};

const PRUint16 g_utBIG5Mapping[] = {
#include "big5.ut"
};

const PRUint16 g_ufBig5HKSCSMapping[] = {
#include "hkscs.uf"
};

const PRUint16 g_ASCIIMapping[] = {
  0x0001, 0x0004, 0x0005, 0x0008, 0x0000, 0x0000, 0x007F, 0x0000
};

const PRUint16 g_utBig5HKSCSMapping[] = {
#include "hkscs.ut"
};

// ucvko
const PRUint16 g_utKSC5601Mapping[] = {
#include "u20kscgl.ut"
};

const PRUint16 g_ufKSC5601Mapping[] = {
#include "u20kscgl.uf"
};

const PRUint16 g_ucvko_AsciiMapping[] = {
  0x0001, 0x0004, 0x0005, 0x0008, 0x0000, 0x0000, 0x007F, 0x0000
};

const PRUint16 g_HangulNullMapping[] ={
  0x0001, 0x0004, 0x0005, 0x0008, 0x0000, 0xAC00, 0xD7A3, 0xAC00
};

const PRUint16 g_ufJohabJamoMapping[] ={   
#include "johabjamo.uf"
};

#else // MOZ_USE_NATIVE_UCONV

#include "nsINativeUConvService.h"
#include "nsNativeUConvService.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(NativeUConvService)

#endif // #ifndef MOZ_USE_NATIVE_UCONV


NS_IMETHODIMP
nsConverterManagerDataRegister(nsIComponentManager* aCompMgr,
                                nsIFile* aPath,
                                const char *aLocation,
                                const char *aType,
                                const nsModuleComponentInfo* aInfo)
{
  return nsCharsetConverterManager::RegisterConverterManagerData();
}

NS_GENERIC_FACTORY_CONSTRUCTOR(nsCharsetConverterManager)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTextToSubURI)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUTF8ConverterService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCharsetAlias2)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsConverterInputStream)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsConverterOutputStream)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPlatformCharset, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsScriptableUnicodeConverter)

static const nsModuleComponentInfo components[] = 
{
  { 
    "Charset Conversion Manager", NS_ICHARSETCONVERTERMANAGER_CID,
    NS_CHARSETCONVERTERMANAGER_CONTRACTID, 
    nsCharsetConverterManagerConstructor,
    nsConverterManagerDataRegister,
  },
  { 
    "Charset Alias Information",  NS_CHARSETALIAS_CID,
    NS_CHARSETALIAS_CONTRACTID, 
    nsCharsetAlias2Constructor 
  },
  { 
    "Text To Sub URI Helper", NS_TEXTTOSUBURI_CID,
    NS_ITEXTTOSUBURI_CONTRACTID, 
    nsTextToSubURIConstructor
  },
  { 
    "Platform Charset Information", NS_PLATFORMCHARSET_CID,
    NS_PLATFORMCHARSET_CONTRACTID, 
    nsPlatformCharsetConstructor
  },
  { "Unicode converter input stream", NS_CONVERTERINPUTSTREAM_CID,              
    NS_CONVERTERINPUTSTREAM_CONTRACTID, 
    nsConverterInputStreamConstructor 
  },   
  { "Unicode converter output stream", NS_CONVERTEROUTPUTSTREAM_CID,
    "@mozilla.org/intl/converter-output-stream;1",
    nsConverterOutputStreamConstructor 
  },   
  { 
    "Unicode Encoder / Decoder for Script", NS_ISCRIPTABLEUNICODECONVERTER_CID,
    NS_ISCRIPTABLEUNICODECONVERTER_CONTRACTID, 
    nsScriptableUnicodeConverterConstructor
  },
#ifdef MOZ_USE_NATIVE_UCONV
  { 
    "Native UConv Service", 
    NS_NATIVE_UCONV_SERVICE_CID,
    NS_NATIVE_UCONV_SERVICE_CONTRACT_ID, 
    NativeUConvServiceConstructor,
  },
#else
  { 
    "Converter to/from UTF8 with charset", NS_UTF8CONVERTERSERVICE_CID,
    NS_UTF8CONVERTERSERVICE_CONTRACTID, 
    nsUTF8ConverterServiceConstructor
  },
  { 
    "ISO-8859-1 To Unicode Converter", NS_ISO88591TOUNICODE_CID, 
    NS_ISO88591TOUNICODE_CONTRACTID,
    nsISO88591ToUnicodeConstructor,
    // global converter registration
    nsUConverterRegSelf, nsUConverterUnregSelf,
  },
  { 
    "windows-1252 To Unicode Converter", NS_CP1252TOUNICODE_CID, 
    NS_CP1252TOUNICODE_CONTRACTID,
    nsCP1252ToUnicodeConstructor,
  },
  { 
    "x-mac-roman To Unicode Converter", NS_MACROMANTOUNICODE_CID,
    NS_MACROMANTOUNICODE_CONTRACTID,
    nsMacRomanToUnicodeConstructor,
  },
  { 
    "UTF-8 To Unicode Converter", NS_UTF8TOUNICODE_CID,
    NS_UTF8TOUNICODE_CONTRACTID,
    nsUTF8ToUnicodeConstructor,
  },
  { 
    "Unicode To ISO-8859-1 Converter", NS_UNICODETOISO88591_CID,
    NS_UNICODETOISO88591_CONTRACTID,
    nsUnicodeToISO88591Constructor, 
  },
  { 
    "Unicode To windows-1252 Converter", NS_UNICODETOCP1252_CID,
    NS_UNICODETOCP1252_CONTRACTID, 
    nsUnicodeToCP1252Constructor, 
  },
  { 
    "Unicode To x-mac-roman Converter", NS_UNICODETOMACROMAN_CID,
    NS_UNICODETOMACROMAN_CONTRACTID, 
    nsUnicodeToMacRomanConstructor, 
  },
  { 
    "Unicode To UTF-8 Converter", NS_UNICODETOUTF8_CID,
    NS_UNICODETOUTF8_CONTRACTID, 
    nsUnicodeToUTF8Constructor, 
  },

  // ucvlatin
  { 
    DECODER_NAME_BASE "us-ascii" , NS_ASCIITOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "us-ascii",
    nsAsciiToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "ISO-8859-2" , NS_ISO88592TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-2",
    nsISO88592ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "ISO-8859-3" , NS_ISO88593TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-3",
    nsISO88593ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "ISO-8859-4" , NS_ISO88594TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-4",
    nsISO88594ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "ISO-8859-5" , NS_ISO88595TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-5",
    nsISO88595ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "ISO-8859-6" , NS_ISO88596TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-6",
    nsISO88596ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "ISO-8859-6-I" , NS_ISO88596ITOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-6-I",
    nsISO88596IToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "ISO-8859-6-E" , NS_ISO88596ETOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-6-E",
    nsISO88596EToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "ISO-8859-7" , NS_ISO88597TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-7",
    nsISO88597ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "ISO-8859-8" , NS_ISO88598TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-8",
    nsISO88598ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "ISO-8859-8-I" , NS_ISO88598ITOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-8-I",
    nsISO88598IToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "ISO-8859-8-E" , NS_ISO88598ETOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-8-E",
    nsISO88598EToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "ISO-8859-9" , NS_ISO88599TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-9",
    nsISO88599ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "ISO-8859-10" , NS_ISO885910TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-10",
    nsISO885910ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "ISO-8859-13" , NS_ISO885913TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-13",
    nsISO885913ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "ISO-8859-14" , NS_ISO885914TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-14",
    nsISO885914ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "ISO-8859-15" , NS_ISO885915TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-15",
    nsISO885915ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "ISO-8859-16" , NS_ISO885916TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-16",
    nsISO885916ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "ISO-IR-111" , NS_ISOIR111TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-IR-111",
    nsISOIR111ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "windows-1250" , NS_CP1250TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "windows-1250",
    nsCP1250ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "windows-1251" , NS_CP1251TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "windows-1251",
    nsCP1251ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "windows-1253" , NS_CP1253TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "windows-1253",
    nsCP1253ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "windows-1254" , NS_CP1254TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "windows-1254",
    nsCP1254ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "windows-1255" , NS_CP1255TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "windows-1255",
    nsCP1255ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "windows-1256" , NS_CP1256TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "windows-1256",
    nsCP1256ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "windows-1257" , NS_CP1257TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "windows-1257",
    nsCP1257ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "windows-1258" , NS_CP1258TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "windows-1258",
    nsCP1258ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "TIS-620" , NS_TIS620TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "TIS-620",
    nsTIS620ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "ISO-8859-11" , NS_ISO885911TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-11",
    nsISO885911ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "windows-874" , NS_CP874TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "windows-874",
    nsCP874ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "IBM866" , NS_CP866TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "IBM866",
    nsCP866ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "KOI8-R" , NS_KOI8RTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "KOI8-R",
    nsKOI8RToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "KOI8-U" , NS_KOI8UTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "KOI8-U",
    nsKOI8UToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "x-mac-ce" , NS_MACCETOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-ce",
    nsMacCEToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "x-mac-greek" , NS_MACGREEKTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-greek",
    nsMacGreekToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "x-mac-turkish" , NS_MACTURKISHTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-turkish",
    nsMacTurkishToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "x-mac-croatian" , NS_MACCROATIANTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-croatian",
    nsMacCroatianToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "x-mac-romanian" , NS_MACROMANIANTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-romanian",
    nsMacRomanianToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "x-mac-cyrillic" , NS_MACCYRILLICTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-cyrillic",
    nsMacCyrillicToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "x-mac-ukrainian" , NS_MACUKRAINIANTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-ukrainian",
    nsMacUkrainianToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "x-mac-icelandic" , NS_MACICELANDICTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-icelandic",
    nsMacIcelandicToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "GEOSTD8" , NS_GEOSTD8TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "GEOSTD8",
    nsGEOSTD8ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "armscii-8" , NS_ARMSCII8TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "armscii-8",
    nsARMSCII8ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "x-viet-tcvn5712" , NS_TCVN5712TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-viet-tcvn5712",
    nsTCVN5712ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "VISCII" , NS_VISCIITOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "VISCII",
    nsVISCIIToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "x-viet-vps" , NS_VPSTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-viet-vps",
    nsVPSToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "UTF-7" , NS_UTF7TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "UTF-7",
    nsUTF7ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "x-imap4-modified-utf7" , NS_MUTF7TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-imap4-modified-utf7",
    nsMUTF7ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "UTF-16" , NS_UTF16TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "UTF-16",
    nsUTF16ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "UTF-16BE" , NS_UTF16BETOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "UTF-16BE",
    nsUTF16BEToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "UTF-16LE" , NS_UTF16LETOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "UTF-16LE",
    nsUTF16LEToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "UTF-32BE" , NS_UTF32BETOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "UTF-32BE",
    nsUTF32BEToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "UTF-32LE" , NS_UTF32LETOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "UTF-32LE",
    nsUTF32LEToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "T.61-8bit" , NS_T61TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "T.61-8bit",
    nsT61ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "x-user-defined" , NS_USERDEFINEDTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-user-defined",
    nsUserDefinedToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "x-mac-arabic" , NS_MACARABICTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-arabic",
    nsMacArabicToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "x-mac-devanagari" , NS_MACDEVANAGARITOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-devanagari",
    nsMacDevanagariToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "x-mac-farsi" , NS_MACFARSITOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-farsi",
    nsMacFarsiToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "x-mac-gurmukhi" , NS_MACGURMUKHITOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-gurmukhi",
    nsMacGurmukhiToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "x-mac-gujarati" , NS_MACGUJARATITOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-gujarati",
    nsMacGujaratiToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "x-mac-hebrew" , NS_MACHEBREWTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-hebrew",
    nsMacHebrewToUnicodeConstructor ,
  },
  { 
    ENCODER_NAME_BASE "us-ascii" , NS_UNICODETOASCII_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "us-ascii",
    nsUnicodeToAsciiConstructor, 
  },
  { 
    ENCODER_NAME_BASE "ISO-8859-2" , NS_UNICODETOISO88592_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-2",
    nsUnicodeToISO88592Constructor, 
  },
  { 
    ENCODER_NAME_BASE "ISO-8859-3" , NS_UNICODETOISO88593_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-3",
    nsUnicodeToISO88593Constructor, 
  },
  { 
    ENCODER_NAME_BASE "ISO-8859-4" , NS_UNICODETOISO88594_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-4",
    nsUnicodeToISO88594Constructor, 
  },
  { 
    ENCODER_NAME_BASE "ISO-8859-5" , NS_UNICODETOISO88595_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-5",
    nsUnicodeToISO88595Constructor, 
  },
  { 
    ENCODER_NAME_BASE "ISO-8859-6" , NS_UNICODETOISO88596_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-6",
    nsUnicodeToISO88596Constructor, 
  },
  { 
    ENCODER_NAME_BASE "ISO-8859-6-I" , NS_UNICODETOISO88596I_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-6-I",
    nsUnicodeToISO88596IConstructor, 
  },
  { 
    ENCODER_NAME_BASE "ISO-8859-6-E" , NS_UNICODETOISO88596E_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-6-E",
    nsUnicodeToISO88596EConstructor, 
  },
  { 
    ENCODER_NAME_BASE "ISO-8859-7" , NS_UNICODETOISO88597_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-7",
    nsUnicodeToISO88597Constructor, 
  },
  { 
    ENCODER_NAME_BASE "ISO-8859-8" , NS_UNICODETOISO88598_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-8",
    nsUnicodeToISO88598Constructor, 
  },
  { 
    ENCODER_NAME_BASE "ISO-8859-8-I" , NS_UNICODETOISO88598I_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-8-I",
    nsUnicodeToISO88598IConstructor, 
  },
  { 
    ENCODER_NAME_BASE "ISO-8859-8-E" , NS_UNICODETOISO88598E_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-8-E",
    nsUnicodeToISO88598EConstructor, 
  },
  { 
    ENCODER_NAME_BASE "ISO-8859-9" , NS_UNICODETOISO88599_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-9",
    nsUnicodeToISO88599Constructor, 
  },
  { 
    ENCODER_NAME_BASE "ISO-8859-10" , NS_UNICODETOISO885910_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-10",
    nsUnicodeToISO885910Constructor, 
  },
  { 
    ENCODER_NAME_BASE "ISO-8859-13" , NS_UNICODETOISO885913_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-13",
    nsUnicodeToISO885913Constructor, 
  },
  { 
    ENCODER_NAME_BASE "ISO-8859-14" , NS_UNICODETOISO885914_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-14",
    nsUnicodeToISO885914Constructor, 
  },
  { 
    ENCODER_NAME_BASE "ISO-8859-15" , NS_UNICODETOISO885915_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-15",
    nsUnicodeToISO885915Constructor, 
  },
  { 
    ENCODER_NAME_BASE "ISO-8859-16" , NS_UNICODETOISO885916_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-16",
    nsUnicodeToISO885916Constructor, 
  },
  { 
    ENCODER_NAME_BASE "ISO-IR-111" , NS_UNICODETOISOIR111_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "ISO-IR-111",
    nsUnicodeToISOIR111Constructor, 
  },
  { 
    ENCODER_NAME_BASE "windows-1250" , NS_UNICODETOCP1250_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "windows-1250",
    nsUnicodeToCP1250Constructor, 
  },
  { 
    ENCODER_NAME_BASE "windows-1251" , NS_UNICODETOCP1251_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "windows-1251",
    nsUnicodeToCP1251Constructor, 
  },
  { 
    ENCODER_NAME_BASE "windows-1253" , NS_UNICODETOCP1253_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "windows-1253",
    nsUnicodeToCP1253Constructor, 
  },
  { 
    ENCODER_NAME_BASE "windows-1254" , NS_UNICODETOCP1254_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "windows-1254",
    nsUnicodeToCP1254Constructor, 
  },
  { 
    ENCODER_NAME_BASE "windows-1255" , NS_UNICODETOCP1255_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "windows-1255",
    nsUnicodeToCP1255Constructor, 
  },
  { 
    ENCODER_NAME_BASE "windows-1256" , NS_UNICODETOCP1256_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "windows-1256",
    nsUnicodeToCP1256Constructor, 
  },
  { 
    ENCODER_NAME_BASE "windows-1257" , NS_UNICODETOCP1257_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "windows-1257",
    nsUnicodeToCP1257Constructor, 
  },
  { 
    ENCODER_NAME_BASE "windows-1258" , NS_UNICODETOCP1258_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "windows-1258",
    nsUnicodeToCP1258Constructor, 
  },
  { 
    ENCODER_NAME_BASE "TIS-620" , NS_UNICODETOTIS620_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "TIS-620",
    nsUnicodeToTIS620Constructor, 
  },
  { 
    ENCODER_NAME_BASE "ISO-8859-11" , NS_UNICODETOISO885911_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-11",
    nsUnicodeToISO885911Constructor ,
  },
  { 
    ENCODER_NAME_BASE "windows-874" , NS_UNICODETOCP874_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "windows-874",
    nsUnicodeToCP874Constructor ,
  },
  { 
    ENCODER_NAME_BASE "IBM866" , NS_UNICODETOCP866_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "IBM866",
    nsUnicodeToCP866Constructor, 
  },
  { 
    ENCODER_NAME_BASE "KOI8-R" , NS_UNICODETOKOI8R_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "KOI8-R",
    nsUnicodeToKOI8RConstructor, 
  },
  { 
    ENCODER_NAME_BASE "KOI8-U" , NS_UNICODETOKOI8U_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "KOI8-U",
    nsUnicodeToKOI8UConstructor, 
  },
  { 
    ENCODER_NAME_BASE "x-mac-ce" , NS_UNICODETOMACCE_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-ce",
    nsUnicodeToMacCEConstructor, 
  },
  { 
    ENCODER_NAME_BASE "x-mac-greek" , NS_UNICODETOMACGREEK_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-greek",
    nsUnicodeToMacGreekConstructor, 
  },
  { 
    ENCODER_NAME_BASE "x-mac-turkish" , NS_UNICODETOMACTURKISH_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-turkish",
    nsUnicodeToMacTurkishConstructor, 
  },
  { 
    ENCODER_NAME_BASE "x-mac-croatian" , NS_UNICODETOMACCROATIAN_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-croatian",
    nsUnicodeToMacCroatianConstructor, 
  },
  { 
    ENCODER_NAME_BASE "x-mac-romanian" , NS_UNICODETOMACROMANIAN_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-romanian",
    nsUnicodeToMacRomanianConstructor, 
  },
  { 
    ENCODER_NAME_BASE "x-mac-cyrillic" , NS_UNICODETOMACCYRILLIC_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-cyrillic",
    nsUnicodeToMacCyrillicConstructor, 
  },
  { 
    ENCODER_NAME_BASE "x-mac-ukrainian" , NS_UNICODETOMACUKRAINIAN_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-ukrainian",
    nsUnicodeToMacUkrainianConstructor, 
  },
  { 
    ENCODER_NAME_BASE "x-mac-icelandic" , NS_UNICODETOMACICELANDIC_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-icelandic",
    nsUnicodeToMacIcelandicConstructor, 
  },
  { 
    ENCODER_NAME_BASE "GEOSTD8" , NS_UNICODETOGEOSTD8_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "GEOSTD8",
    nsUnicodeToGEOSTD8Constructor, 
  },
  { 
    ENCODER_NAME_BASE "armscii-8" , NS_UNICODETOARMSCII8_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "armscii-8",
    nsUnicodeToARMSCII8Constructor, 
  },
  { 
    ENCODER_NAME_BASE "x-viet-tcvn5712" , NS_UNICODETOTCVN5712_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-viet-tcvn5712",
    nsUnicodeToTCVN5712Constructor, 
  },
  { 
    ENCODER_NAME_BASE "VISCII" , NS_UNICODETOVISCII_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "VISCII",
    nsUnicodeToVISCIIConstructor, 
  },
  { 
    ENCODER_NAME_BASE "x-viet-vps" , NS_UNICODETOVPS_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-viet-vps",
    nsUnicodeToVPSConstructor, 
  },
  { 
    ENCODER_NAME_BASE "UTF-7" , NS_UNICODETOUTF7_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "UTF-7",
    nsUnicodeToUTF7Constructor, 
  },
  { 
    ENCODER_NAME_BASE "x-imap4-modified-utf7" , NS_UNICODETOMUTF7_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-imap4-modified-utf7",
    nsUnicodeToMUTF7Constructor, 
  },
  { 
    ENCODER_NAME_BASE "UTF-16BE" , NS_UNICODETOUTF16BE_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "UTF-16BE",
    nsUnicodeToUTF16BEConstructor, 
  },
  { 
    ENCODER_NAME_BASE "UTF-16LE" , NS_UNICODETOUTF16LE_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "UTF-16LE",
    nsUnicodeToUTF16LEConstructor, 
  },
  { 
    ENCODER_NAME_BASE "UTF-16" , NS_UNICODETOUTF16_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "UTF-16",
    nsUnicodeToUTF16Constructor, 
  },
  { 
    ENCODER_NAME_BASE "UTF-32BE" , NS_UNICODETOUTF32BE_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "UTF-32BE",
    nsUnicodeToUTF32BEConstructor, 
  },
  { 
    ENCODER_NAME_BASE "UTF-32LE" , NS_UNICODETOUTF32LE_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "UTF-32LE",
    nsUnicodeToUTF32LEConstructor, 
  },
    { 
    ENCODER_NAME_BASE "T.61-8bit" , NS_UNICODETOT61_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "T.61-8bit",
    nsUnicodeToT61Constructor, 
  },
  { 
    ENCODER_NAME_BASE "x-user-defined" , NS_UNICODETOUSERDEFINED_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-user-defined",
    nsUnicodeToUserDefinedConstructor, 
  },
  { 
    ENCODER_NAME_BASE "Adobe-Symbol-Encoding" , NS_UNICODETOSYMBOL_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "Adobe-Symbol-Encoding",
    nsUnicodeToSymbolConstructor, 
  },
  { 
    ENCODER_NAME_BASE "x-zapf-dingbats" , NS_UNICODETOZAPFDINGBATS_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-zapf-dingbats",
    nsUnicodeToZapfDingbatConstructor, 
  },
  { 
    ENCODER_NAME_BASE "x-adobe-euro", NS_UNICODETOADOBEEURO_CID,
    NS_UNICODEENCODER_CONTRACTID_BASE "x-adobe-euro",
    nsUnicodeToAdobeEuroConstructor, 
  },
  { 
    ENCODER_NAME_BASE "x-mac-arabic" , NS_UNICODETOMACARABIC_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-arabic",
    nsUnicodeToMacArabicConstructor, 
  },
  { 
    ENCODER_NAME_BASE "x-mac-devanagari" , NS_UNICODETOMACDEVANAGARI_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-devanagari",
    nsUnicodeToMacDevanagariConstructor, 
  },
  { 
    ENCODER_NAME_BASE "x-mac-farsi" , NS_UNICODETOMACFARSI_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-farsi",
    nsUnicodeToMacFarsiConstructor, 
  },
  { 
    ENCODER_NAME_BASE "x-mac-gurmukhi" , NS_UNICODETOMACGURMUKHI_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-gurmukhi",
    nsUnicodeToMacGurmukhiConstructor, 
  },
  { 
    ENCODER_NAME_BASE "x-mac-gujarati" , NS_UNICODETOMACGUJARATI_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-gujarati",
    nsUnicodeToMacGujaratiConstructor, 
  },
  { 
    ENCODER_NAME_BASE "x-mac-hebrew" , NS_UNICODETOMACHEBREW_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-hebrew",
    nsUnicodeToMacHebrewConstructor, 
  },
  { 
    ENCODER_NAME_BASE "x-tscii" , NS_UNICODETOTSCII_CID,
    NS_UNICODEENCODER_CONTRACTID_BASE "x-tscii",
    nsUnicodeToTSCIIConstructor, 
  },
  { 
    ENCODER_NAME_BASE "x-tamilttf-0" , NS_UNICODETOTAMILTTF_CID,
    NS_UNICODEENCODER_CONTRACTID_BASE "x-tamilttf-0",
    nsUnicodeToTamilTTFConstructor, 
  },
#ifdef MOZ_EXTRA_X11CONVERTERS
  { 
    ENCODER_NAME_BASE "x-iso-8859-6-8-x" , NS_UNICODETOLANGBOXARABIC_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-iso-8859-6-8-x",
    nsUnicodeToLangBoxArabic8Constructor, 
  },
  { 
    ENCODER_NAME_BASE "x-iso-8859-6-16" , NS_UNICODETOLANGBOXARABIC16_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-iso-8859-6-16",
    nsUnicodeToLangBoxArabic16Constructor, 
  },
#endif // MOZ_EXTRA_X11CONVERTERS
  // ucvibm
  { 
    DECODER_NAME_BASE "IBM850" , NS_CP850TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "IBM850",
    nsCP850ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "IBM852" , NS_CP852TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "IBM852",
    nsCP852ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "IBM855" , NS_CP855TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "IBM855",
    nsCP855ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "IBM857" , NS_CP857TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "IBM857",
    nsCP857ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "IBM862" , NS_CP862TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "IBM862",
    nsCP862ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "IBM864" , NS_CP864TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "IBM864",
    nsCP864ToUnicodeConstructor ,
  },
  { 
    DECODER_NAME_BASE "IBM864i" , NS_CP864ITOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "IBM864i",
    nsCP864iToUnicodeConstructor ,
  },
#ifdef MOZ_EXTRA_X11CONVERTERS
  {
    DECODER_NAME_BASE "x-IBM1046" , NS_CP1046TOUNICODE_CID,
    NS_UNICODEDECODER_CONTRACTID_BASE "x-IBM1046",
    nsCP1046ToUnicodeConstructor ,
  },
#endif
#ifdef XP_OS2
  {
    DECODER_NAME_BASE "IBM869" , NS_CP869TOUNICODE_CID,
    NS_UNICODEDECODER_CONTRACTID_BASE "IBM869",
    nsCP869ToUnicodeConstructor ,
  },
  {
    DECODER_NAME_BASE "IBM1125" , NS_CP1125TOUNICODE_CID,
    NS_UNICODEDECODER_CONTRACTID_BASE "IBM1125",
    nsCP1125ToUnicodeConstructor ,
  },
  {
    DECODER_NAME_BASE "IBM1131" , NS_CP1131TOUNICODE_CID,
    NS_UNICODEDECODER_CONTRACTID_BASE "IBM1131",
    nsCP1131ToUnicodeConstructor ,
  },
#endif
  { 
    ENCODER_NAME_BASE "IBM850" , NS_UNICODETOCP850_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "IBM850",
    nsUnicodeToCP850Constructor, 
  },
  { 
    ENCODER_NAME_BASE "IBM852" , NS_UNICODETOCP852_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "IBM852",
    nsUnicodeToCP852Constructor, 
  },
  { 
    ENCODER_NAME_BASE "IBM855" , NS_UNICODETOCP855_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "IBM855",
    nsUnicodeToCP855Constructor, 
  },
  { 
    ENCODER_NAME_BASE "IBM857" , NS_UNICODETOCP857_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "IBM857",
    nsUnicodeToCP857Constructor, 
  },
  { 
    ENCODER_NAME_BASE "IBM862" , NS_UNICODETOCP862_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "IBM862",
    nsUnicodeToCP862Constructor, 
  },
  { 
    ENCODER_NAME_BASE "IBM864" , NS_UNICODETOCP864_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "IBM864",
    nsUnicodeToCP864Constructor, 
  },
  { 
    ENCODER_NAME_BASE "IBM864i" , NS_UNICODETOCP864I_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "IBM864i",
    nsUnicodeToCP864iConstructor, 
  },
#ifdef MOZ_EXTRA_X11CONVERTERS
  {
    ENCODER_NAME_BASE "x-IBM1046" , NS_UNICODETOCP1046_CID,
    NS_UNICODEENCODER_CONTRACTID_BASE "x-IBM1046",
    nsUnicodeToCP1046Constructor,
  },
#endif
#ifdef XP_OS2
  {
    ENCODER_NAME_BASE "IBM869" , NS_UNICODETOCP869_CID,
    NS_UNICODEENCODER_CONTRACTID_BASE "IBM869",
    nsUnicodeToCP869Constructor,
  },
  {
    ENCODER_NAME_BASE "IBM1125" , NS_UNICODETOCP1125_CID,
    NS_UNICODEENCODER_CONTRACTID_BASE "IBM1125",
    nsUnicodeToCP1125Constructor,
  },
  {
    ENCODER_NAME_BASE "IBM1131" , NS_UNICODETOCP1131_CID,
    NS_UNICODEENCODER_CONTRACTID_BASE "IBM1131",
    nsUnicodeToCP1131Constructor,
  },
#endif
    // ucvja
  { 
    DECODER_NAME_BASE "Shift_JIS" , NS_SJISTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "Shift_JIS",
    nsShiftJISToUnicodeConstructor ,
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
#ifdef MOZ_EXTRA_X11CONVERTERS
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
#endif

  // ucvtw2
  { 
    DECODER_NAME_BASE "x-euc-tw" , NS_EUCTWTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-euc-tw",
    nsEUCTWToUnicodeConstructor,
  },
  { 
    ENCODER_NAME_BASE "x-euc-tw" , NS_UNICODETOEUCTW_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-euc-tw",
    nsUnicodeToEUCTWConstructor,
  },
#ifdef MOZ_EXTRA_X11CONVERTERS
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
#endif

  // ucvtw
  { 
    ENCODER_NAME_BASE "Big5" , NS_UNICODETOBIG5_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "Big5",
    nsUnicodeToBIG5Constructor,
  },
#ifdef MOZ_EXTRA_X11CONVERTERS
  { 
    ENCODER_NAME_BASE "x-x-big5" , NS_UNICODETOBIG5NOASCII_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-x-big5",
    nsUnicodeToBIG5NoAsciiConstructor,
  },
#endif
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
  },
  { 
    ENCODER_NAME_BASE "EUC-KR" , NS_UNICODETOEUCKR_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "EUC-KR",
    nsUnicodeToEUCKRConstructor, 
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
#ifdef MOZ_EXTRA_X11CONVERTERS
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
    ENCODER_NAME_BASE "x-johab-noascii", NS_UNICODETOJOHABNOASCII_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-johab-noascii",
    nsUnicodeToJohabNoAsciiConstructor,
  },
#endif
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
  },
  { 
    ENCODER_NAME_BASE "x-koreanjamo-0" , NS_UNICODETOJAMOTTF_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-koreanjamo-0",
    nsUnicodeToJamoTTFConstructor,
  },
  // ucvcn
  { 
    DECODER_NAME_BASE "GB2312" , NS_GB2312TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "GB2312",
    nsGB2312ToUnicodeV2Constructor ,
  },
  { 
    ENCODER_NAME_BASE "GB2312" , NS_UNICODETOGB2312_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "GB2312",
    nsUnicodeToGB2312V2Constructor, 
  },
  { 
    DECODER_NAME_BASE "windows-936" , NS_CP936TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "windows-936",
    nsCP936ToUnicodeConstructor ,
  },
  { 
    ENCODER_NAME_BASE "windows-936" , NS_UNICODETOCP936_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "windows-936",
    nsUnicodeToCP936Constructor, 
  },
  { 
    DECODER_NAME_BASE "x-gbk" , NS_GBKTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-gbk",
    nsGBKToUnicodeConstructor ,
  },
  { 
    ENCODER_NAME_BASE "x-gbk" , NS_UNICODETOGBK_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-gbk",
    nsUnicodeToGBKConstructor, 
  },  
#ifdef MOZ_EXTRA_X11CONVERTERS
  { 
    ENCODER_NAME_BASE "x-gbk-noascii" , NS_UNICODETOGBKNOASCII_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-gbk-noascii",
    nsUnicodeToGBKNoAsciiConstructor, 
  },  
#endif
  { 
    DECODER_NAME_BASE "HZ-GB-2312" , NS_HZTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "HZ-GB-2312",
    nsHZToUnicodeConstructor ,
  },  
  { 
    ENCODER_NAME_BASE "HZ-GB-2312" , NS_UNICODETOHZ_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "HZ-GB-2312",
    nsUnicodeToHZConstructor, 
  },  
#ifdef MOZ_EXTRA_X11CONVERTERS
  { 
    ENCODER_NAME_BASE "gb_2312-80" , NS_UNICODETOGB2312GL_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "gb_2312-80",
    nsUnicodeToGB2312GLConstructor, 
  },
#endif
  { 
    DECODER_NAME_BASE "gb18030" , NS_GB18030TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "gb18030",
    nsGB18030ToUnicodeConstructor ,
  },  
#ifdef MOZ_EXTRA_X11CONVERTERS
  { 
    ENCODER_NAME_BASE "gb18030.2000-0" , NS_UNICODETOGB18030Font0_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "gb18030.2000-0",
    nsUnicodeToGB18030Font0Constructor, 
  },  
  { 
    ENCODER_NAME_BASE "gb18030.2000-1" , NS_UNICODETOGB18030Font1_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "gb18030.2000-1",
    nsUnicodeToGB18030Font1Constructor, 
  },  
#endif
  { 
    ENCODER_NAME_BASE "gb18030" , NS_UNICODETOGB18030_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "gb18030",
    nsUnicodeToGB18030Constructor, 
  },
  {
    DECODER_NAME_BASE "ISO-2022-CN" , NS_ISO2022CNTOUNICODE_CID,
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-2022-CN",
    nsISO2022CNToUnicodeConstructor,
  },
#endif // MOZ_USE_NATIVE_UCONV
};

NS_IMPL_NSGETMODULE(nsUConvModule, components)

