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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#define NS_IMPL_IDS

#include "pratom.h"
#include "nspr.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIFactory.h"
#include "nsIRegistry.h"
#include "nsIGenericFactory.h"
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
#include "nsIModule.h"
#include "nsUCvLatinCID.h"
#include "nsUCvLatinDll.h"

#include "nsUEscapeToUnicode.h"
#include "nsUnicodeToUEscape.h"
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
#include "nsARMSCII8ToUnicode.h"
#include "nsTCVN5712ToUnicode.h"
#include "nsVISCIIToUnicode.h"
#include "nsVPSToUnicode.h"
#include "nsUTF7ToUnicode.h"
#include "nsMUTF7ToUnicode.h"
#include "nsUCS4BEToUnicode.h"
#include "nsUCS4LEToUnicode.h"
#include "nsUCS2BEToUnicode.h"
#include "nsUCS2LEToUnicode.h"
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
#include "nsUnicodeToARMSCII8.h"
#include "nsUnicodeToTCVN5712.h"
#include "nsUnicodeToVISCII.h"
#include "nsUnicodeToVPS.h"
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


//----------------------------------------------------------------------------
// Global functions and data [declaration]

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

#define DECODER_NAME_BASE "Unicode Decoder-"
#define ENCODER_NAME_BASE "Unicode Encoder-"

NS_IMPL_NSUCONVERTERREGSELF

NS_UCONV_REG_UNREG(nsAsciiToUnicode, "us-ascii", "Unicode" , NS_ASCIITOUNICODE_CID);
NS_UCONV_REG_UNREG(nsUEscapeToUnicode, "x-u-escaped", "Unicode" , NS_UESCAPETOUNICODE_CID);
NS_UCONV_REG_UNREG(nsISO88592ToUnicode, "ISO-8859-2", "Unicode" , NS_ISO88592TOUNICODE_CID);
NS_UCONV_REG_UNREG(nsISO88593ToUnicode, "ISO-8859-3", "Unicode" , NS_ISO88593TOUNICODE_CID);
NS_UCONV_REG_UNREG(nsISO88594ToUnicode, "ISO-8859-4", "Unicode" , NS_ISO88594TOUNICODE_CID);
NS_UCONV_REG_UNREG(nsISO88595ToUnicode, "ISO-8859-5", "Unicode" , NS_ISO88595TOUNICODE_CID);
NS_UCONV_REG_UNREG(nsISO88596ToUnicode, "ISO-8859-6", "Unicode" , NS_ISO88596TOUNICODE_CID);
NS_UCONV_REG_UNREG(nsISO88596IToUnicode, "ISO-8859-6-I", "Unicode" , NS_ISO88596ITOUNICODE_CID);
NS_UCONV_REG_UNREG(nsISO88596EToUnicode, "ISO-8859-6-E", "Unicode" , NS_ISO88596ETOUNICODE_CID);
NS_UCONV_REG_UNREG(nsISO88597ToUnicode, "ISO-8859-7", "Unicode" , NS_ISO88597TOUNICODE_CID);
NS_UCONV_REG_UNREG(nsISO88598ToUnicode, "ISO-8859-8", "Unicode" , NS_ISO88598TOUNICODE_CID);
NS_UCONV_REG_UNREG(nsISO88598IToUnicode, "ISO-8859-8-I", "Unicode" , NS_ISO88598ITOUNICODE_CID);
NS_UCONV_REG_UNREG(nsISO88598EToUnicode, "ISO-8859-8-E", "Unicode" , NS_ISO88598ETOUNICODE_CID);
NS_UCONV_REG_UNREG(nsISO88599ToUnicode, "ISO-8859-9", "Unicode" , NS_ISO88599TOUNICODE_CID);
NS_UCONV_REG_UNREG(nsISO885910ToUnicode, "ISO-8859-10", "Unicode" , NS_ISO885910TOUNICODE_CID);
NS_UCONV_REG_UNREG(nsISO885913ToUnicode, "ISO-8859-13", "Unicode" , NS_ISO885913TOUNICODE_CID);
NS_UCONV_REG_UNREG(nsISO885914ToUnicode, "ISO-8859-14", "Unicode" , NS_ISO885914TOUNICODE_CID);
NS_UCONV_REG_UNREG(nsISO885915ToUnicode, "ISO-8859-15", "Unicode" , NS_ISO885915TOUNICODE_CID);
NS_UCONV_REG_UNREG(nsISOIR111ToUnicode, "ISO-IR-111", "Unicode" , NS_ISOIR111TOUNICODE_CID);
NS_UCONV_REG_UNREG(nsCP1250ToUnicode, "windows-1250", "Unicode" , NS_CP1250TOUNICODE_CID);
NS_UCONV_REG_UNREG(nsCP1251ToUnicode, "windows-1251", "Unicode" , NS_CP1251TOUNICODE_CID);
NS_UCONV_REG_UNREG(nsCP1253ToUnicode, "windows-1253", "Unicode" , NS_CP1253TOUNICODE_CID);
NS_UCONV_REG_UNREG(nsCP1254ToUnicode, "windows-1254", "Unicode" , NS_CP1254TOUNICODE_CID);
NS_UCONV_REG_UNREG(nsCP1255ToUnicode, "windows-1255", "Unicode" , NS_CP1255TOUNICODE_CID);
NS_UCONV_REG_UNREG(nsCP1256ToUnicode, "windows-1256", "Unicode" , NS_CP1256TOUNICODE_CID);
NS_UCONV_REG_UNREG(nsCP1257ToUnicode, "windows-1257", "Unicode" , NS_CP1257TOUNICODE_CID);
NS_UCONV_REG_UNREG(nsCP1258ToUnicode, "windows-1258", "Unicode" , NS_CP1258TOUNICODE_CID);
NS_UCONV_REG_UNREG(nsCP874ToUnicode, "TIS-620", "Unicode" , NS_CP874TOUNICODE_CID);
NS_UCONV_REG_UNREG(nsCP866ToUnicode, "IBM866", "Unicode" , NS_CP866TOUNICODE_CID);
NS_UCONV_REG_UNREG(nsKOI8RToUnicode, "KOI8-R", "Unicode" , NS_KOI8RTOUNICODE_CID);
NS_UCONV_REG_UNREG(nsKOI8UToUnicode, "KOI8-U", "Unicode" , NS_KOI8UTOUNICODE_CID);
NS_UCONV_REG_UNREG(nsMacCEToUnicode, "x-mac-ce", "Unicode" , NS_MACCETOUNICODE_CID);
NS_UCONV_REG_UNREG(nsMacGreekToUnicode, "x-mac-greek", "Unicode" , NS_MACGREEKTOUNICODE_CID);
NS_UCONV_REG_UNREG(nsMacTurkishToUnicode, "x-mac-turkish", "Unicode" , NS_MACTURKISHTOUNICODE_CID);
NS_UCONV_REG_UNREG(nsMacCroatianToUnicode, "x-mac-croatian", "Unicode" , NS_MACCROATIANTOUNICODE_CID);
NS_UCONV_REG_UNREG(nsMacRomanianToUnicode, "x-mac-romanian", "Unicode" , NS_MACROMANIANTOUNICODE_CID);
NS_UCONV_REG_UNREG(nsMacCyrillicToUnicode, "x-mac-cyrillic", "Unicode" , NS_MACCYRILLICTOUNICODE_CID);
NS_UCONV_REG_UNREG(nsMacUkrainianToUnicode, "x-mac-ukrainian", "Unicode" , NS_MACUKRAINIANTOUNICODE_CID);
NS_UCONV_REG_UNREG(nsMacIcelandicToUnicode, "x-mac-icelandic", "Unicode" , NS_MACICELANDICTOUNICODE_CID);
NS_UCONV_REG_UNREG(nsARMSCII8ToUnicode, "armscii-8", "Unicode" , NS_ARMSCII8TOUNICODE_CID);
NS_UCONV_REG_UNREG(nsTCVN5712ToUnicode, "x-viet-tcvn5712", "Unicode" , NS_TCVN5712TOUNICODE_CID);
NS_UCONV_REG_UNREG(nsVISCIIToUnicode, "VISCII", "Unicode" , NS_VISCIITOUNICODE_CID);
NS_UCONV_REG_UNREG(nsVPSToUnicode, "x-viet-vps", "Unicode" , NS_VPSTOUNICODE_CID);
NS_UCONV_REG_UNREG(nsUTF7ToUnicode, "UTF-7", "Unicode" , NS_UTF7TOUNICODE_CID);
NS_UCONV_REG_UNREG(nsMUTF7ToUnicode, "x-imap4-modified-utf7", "Unicode" , NS_MUTF7TOUNICODE_CID);
NS_UCONV_REG_UNREG(nsUTF16BEToUnicode, "UTF-16BE", "Unicode" , NS_UTF16BETOUNICODE_CID);
NS_UCONV_REG_UNREG(nsUTF16LEToUnicode, "UTF-16LE", "Unicode" , NS_UTF16LETOUNICODE_CID);
NS_UCONV_REG_UNREG(nsUCS4BEToUnicode, "UTF-32BE", "Unicode" , NS_UTF32BETOUNICODE_CID);
NS_UCONV_REG_UNREG(nsUCS4LEToUnicode, "UTF-32LE", "Unicode" , NS_UTF32LETOUNICODE_CID);
NS_UCONV_REG_UNREG(nsT61ToUnicode, "T.61-8bit", "Unicode" , NS_T61TOUNICODE_CID);
NS_UCONV_REG_UNREG(nsUserDefinedToUnicode, "x-user-defined", "Unicode" , NS_USERDEFINEDTOUNICODE_CID);
NS_UCONV_REG_UNREG(nsUnicodeToAscii, "Unicode", "us-ascii" , NS_UNICODETOASCII_CID);
NS_UCONV_REG_UNREG(nsUnicodeToUEscape, "Unicode", "x-u-escaped" , NS_UNICODETOUESCAPE_CID);
NS_UCONV_REG_UNREG(nsUnicodeToISO88592, "Unicode", "ISO-8859-2" , NS_UNICODETOISO88592_CID);
NS_UCONV_REG_UNREG(nsUnicodeToISO88593, "Unicode", "ISO-8859-3" , NS_UNICODETOISO88593_CID);
NS_UCONV_REG_UNREG(nsUnicodeToISO88594, "Unicode", "ISO-8859-4" , NS_UNICODETOISO88594_CID);
NS_UCONV_REG_UNREG(nsUnicodeToISO88595, "Unicode", "ISO-8859-5" , NS_UNICODETOISO88595_CID);
NS_UCONV_REG_UNREG(nsUnicodeToISO88596, "Unicode", "ISO-8859-6" , NS_UNICODETOISO88596_CID);
NS_UCONV_REG_UNREG(nsUnicodeToISO88596I, "Unicode", "ISO-8859-6-I" , NS_UNICODETOISO88596I_CID);
NS_UCONV_REG_UNREG(nsUnicodeToISO88596E, "Unicode", "ISO-8859-6-E" , NS_UNICODETOISO88596E_CID);
NS_UCONV_REG_UNREG(nsUnicodeToISO88597, "Unicode", "ISO-8859-7" , NS_UNICODETOISO88597_CID);
NS_UCONV_REG_UNREG(nsUnicodeToISO88598, "Unicode", "ISO-8859-8" , NS_UNICODETOISO88598_CID);
NS_UCONV_REG_UNREG(nsUnicodeToISO88598I, "Unicode", "ISO-8859-8-I" , NS_UNICODETOISO88598I_CID);
NS_UCONV_REG_UNREG(nsUnicodeToISO88598E, "Unicode", "ISO-8859-8-E" , NS_UNICODETOISO88598E_CID);
NS_UCONV_REG_UNREG(nsUnicodeToISO88599, "Unicode", "ISO-8859-9" , NS_UNICODETOISO88599_CID);
NS_UCONV_REG_UNREG(nsUnicodeToISO885910, "Unicode", "ISO-8859-10" , NS_UNICODETOISO885910_CID);
NS_UCONV_REG_UNREG(nsUnicodeToISO885913, "Unicode", "ISO-8859-13" , NS_UNICODETOISO885913_CID);
NS_UCONV_REG_UNREG(nsUnicodeToISO885914, "Unicode", "ISO-8859-14" , NS_UNICODETOISO885914_CID);
NS_UCONV_REG_UNREG(nsUnicodeToISO885915, "Unicode", "ISO-8859-15" , NS_UNICODETOISO885915_CID);
NS_UCONV_REG_UNREG(nsUnicodeToISOIR111, "Unicode", "ISO-IR-111" , NS_UNICODETOISOIR111_CID);
NS_UCONV_REG_UNREG(nsUnicodeToCP1250, "Unicode", "windows-1250" , NS_UNICODETOCP1250_CID);
NS_UCONV_REG_UNREG(nsUnicodeToCP1251, "Unicode", "windows-1251" , NS_UNICODETOCP1251_CID);
NS_UCONV_REG_UNREG(nsUnicodeToCP1253, "Unicode", "windows-1253" , NS_UNICODETOCP1253_CID);
NS_UCONV_REG_UNREG(nsUnicodeToCP1254, "Unicode", "windows-1254" , NS_UNICODETOCP1254_CID);
NS_UCONV_REG_UNREG(nsUnicodeToCP1255, "Unicode", "windows-1255" , NS_UNICODETOCP1255_CID);
NS_UCONV_REG_UNREG(nsUnicodeToCP1256, "Unicode", "windows-1256" , NS_UNICODETOCP1256_CID);
NS_UCONV_REG_UNREG(nsUnicodeToCP1257, "Unicode", "windows-1257" , NS_UNICODETOCP1257_CID);
NS_UCONV_REG_UNREG(nsUnicodeToCP1258, "Unicode", "windows-1258" , NS_UNICODETOCP1258_CID);
NS_UCONV_REG_UNREG(nsUnicodeToCP874, "Unicode", "TIS-620" , NS_UNICODETOCP874_CID);
NS_UCONV_REG_UNREG(nsUnicodeToCP866, "Unicode", "IBM866" , NS_UNICODETOCP866_CID);
NS_UCONV_REG_UNREG(nsUnicodeToKOI8R, "Unicode", "KOI8-R" , NS_UNICODETOKOI8R_CID);
NS_UCONV_REG_UNREG(nsUnicodeToKOI8U, "Unicode", "KOI8-U" , NS_UNICODETOKOI8U_CID);
NS_UCONV_REG_UNREG(nsUnicodeToMacCE, "Unicode", "x-mac-ce" , NS_UNICODETOMACCE_CID);
NS_UCONV_REG_UNREG(nsUnicodeToMacGreek, "Unicode", "x-mac-greek" , NS_UNICODETOMACGREEK_CID);
NS_UCONV_REG_UNREG(nsUnicodeToMacTurkish, "Unicode", "x-mac-turkish" , NS_UNICODETOMACTURKISH_CID);
NS_UCONV_REG_UNREG(nsUnicodeToMacCroatian, "Unicode", "x-mac-croatian" , NS_UNICODETOMACCROATIAN_CID);
NS_UCONV_REG_UNREG(nsUnicodeToMacRomanian, "Unicode", "x-mac-romanian" , NS_UNICODETOMACROMANIAN_CID);
NS_UCONV_REG_UNREG(nsUnicodeToMacCyrillic, "Unicode", "x-mac-cyrillic" , NS_UNICODETOMACCYRILLIC_CID);
NS_UCONV_REG_UNREG(nsUnicodeToMacUkrainian, "Unicode", "x-mac-ukrainian" , NS_UNICODETOMACUKRAINIAN_CID);
NS_UCONV_REG_UNREG(nsUnicodeToMacIcelandic, "Unicode", "x-mac-icelandic" , NS_UNICODETOMACICELANDIC_CID);
NS_UCONV_REG_UNREG(nsUnicodeToARMSCII8, "Unicode", "armscii-8" , NS_UNICODETOARMSCII8_CID);
NS_UCONV_REG_UNREG(nsUnicodeToTCVN5712, "Unicode", "x-viet-tcvn5712" , NS_UNICODETOTCVN5712_CID);
NS_UCONV_REG_UNREG(nsUnicodeToVISCII, "Unicode", "VISCII" , NS_UNICODETOVISCII_CID);
NS_UCONV_REG_UNREG(nsUnicodeToVPS, "Unicode", "x-viet-vps" , NS_UNICODETOVPS_CID);
NS_UCONV_REG_UNREG(nsUnicodeToUTF7, "Unicode", "UTF-7" , NS_UNICODETOUTF7_CID);
NS_UCONV_REG_UNREG(nsUnicodeToMUTF7, "Unicode", "x-imap4-modified-utf7" , NS_UNICODETOMUTF7_CID);
NS_UCONV_REG_UNREG(nsUnicodeToUTF16BE, "Unicode", "UTF-16BE" , NS_UNICODETOUTF16BE_CID);
NS_UCONV_REG_UNREG(nsUnicodeToUTF16LE, "Unicode", "UTF-16LE" , NS_UNICODETOUTF16LE_CID);
NS_UCONV_REG_UNREG(nsUnicodeToUTF16, "Unicode", "UTF-16" , NS_UNICODETOUTF16_CID);
NS_UCONV_REG_UNREG(nsUnicodeToUCS4BE, "Unicode", "UTF-32BE" , NS_UNICODETOUTF32BE_CID);
NS_UCONV_REG_UNREG(nsUnicodeToUCS4LE, "Unicode", "UTF-32LE" , NS_UNICODETOUTF32LE_CID);
NS_UCONV_REG_UNREG(nsUnicodeToT61, "Unicode", "T.61-8bit" , NS_UNICODETOT61_CID);
NS_UCONV_REG_UNREG(nsUnicodeToUserDefined, "Unicode", "x-user-defined" , NS_UNICODETOUSERDEFINED_CID);
NS_UCONV_REG_UNREG(nsUnicodeToSymbol, "Unicode", "Adobe-Symbol-Encoding" , NS_UNICODETOSYMBOL_CID);
NS_UCONV_REG_UNREG(nsUnicodeToZapfDingbat, "Unicode", "x-zapf-dingbats" , NS_UNICODETOZAPFDINGBATS_CID);
NS_UCONV_REG_UNREG(nsMacArabicToUnicode,  "x-mac-arabic" , "Unicode" , NS_MACARABICTOUNICODE_CID);
NS_UCONV_REG_UNREG(nsMacDevanagariToUnicode,  "x-mac-devanagari" , "Unicode" , NS_MACDEVANAGARITOUNICODE_CID);
NS_UCONV_REG_UNREG(nsMacFarsiToUnicode,  "x-mac-farsi" , "Unicode" , NS_MACFARSITOUNICODE_CID);
NS_UCONV_REG_UNREG(nsMacGurmukhiToUnicode,  "x-mac-gurmukhi" , "Unicode" , NS_MACGURMUKHITOUNICODE_CID);
NS_UCONV_REG_UNREG(nsMacGujaratiToUnicode,  "x-mac-gujarati" , "Unicode" , NS_MACGUJARATITOUNICODE_CID);
NS_UCONV_REG_UNREG(nsMacHebrewToUnicode,  "x-mac-hebrew" , "Unicode" , NS_MACHEBREWTOUNICODE_CID);
NS_UCONV_REG_UNREG(nsUnicodeToMacArabic,  "Unicode" , "x-mac-arabic" , NS_UNICODETOMACARABIC_CID);
NS_UCONV_REG_UNREG(nsUnicodeToMacDevanagari,  "Unicode" , "x-mac-devanagari" , NS_UNICODETOMACDEVANAGARI_CID);
NS_UCONV_REG_UNREG(nsUnicodeToMacFarsi,  "Unicode" , "x-mac-farsi" , NS_UNICODETOMACFARSI_CID);
NS_UCONV_REG_UNREG(nsUnicodeToMacGurmukhi,  "Unicode" , "x-mac-gurmukhi" , NS_UNICODETOMACGURMUKHI_CID);
NS_UCONV_REG_UNREG(nsUnicodeToMacGujarati,  "Unicode" , "x-mac-gujarati" , NS_UNICODETOMACGUJARATI_CID);
NS_UCONV_REG_UNREG(nsUnicodeToMacHebrew,  "Unicode" , "x-mac-hebrew" , NS_UNICODETOMACHEBREW_CID);

NS_GENERIC_FACTORY_CONSTRUCTOR(nsAsciiToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUEscapeToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsISO88592ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsISO88593ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsISO88594ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsISO88595ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsISO88596ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsISO88596EToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsISO88596IToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsISO88597ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsISO88598ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsISO88598EToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsISO88598IToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsISO88599ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsISO885910ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsISO885913ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsISO885914ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsISO885915ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsISOIR111ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCP1250ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCP1251ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCP1253ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCP1254ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCP1255ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCP1256ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCP1257ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCP1258ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCP874ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCP866ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsKOI8RToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsKOI8UToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMacCEToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMacGreekToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMacTurkishToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMacCroatianToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMacRomanianToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMacCyrillicToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMacUkrainianToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMacIcelandicToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsARMSCII8ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTCVN5712ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsVISCIIToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsVPSToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUTF7ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMUTF7ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUTF16BEToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUTF16LEToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUCS4BEToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUCS4LEToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsT61ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUserDefinedToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToAscii);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToUEscape);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToISO88592);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToISO88593);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToISO88594);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToISO88595);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToISO88596);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToISO88596E);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToISO88596I);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToISO88597);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToISO88598);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToISO88598E);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToISO88598I);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToISO88599);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToISO885910);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToISO885913);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToISO885914);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToISO885915);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToISOIR111);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToCP1250);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToCP1251);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToCP1253);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToCP1254);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToCP1255);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToCP1256);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToCP1257);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToCP1258);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToCP874);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToCP866);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToKOI8R);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToKOI8U);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToMacCE);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToMacGreek);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToMacTurkish);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToMacCroatian);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToMacRomanian);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToMacCyrillic);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToMacUkrainian);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToMacIcelandic);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToARMSCII8);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToTCVN5712);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToVISCII);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToVPS);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToUTF7);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToMUTF7);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToUTF16BE);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToUTF16LE);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToUTF16);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToUCS4BE);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToUCS4LE);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToT61);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToUserDefined);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToSymbol);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToZapfDingbat);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMacArabicToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMacDevanagariToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMacFarsiToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMacGurmukhiToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMacGujaratiToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMacHebrewToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToMacArabic);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToMacDevanagari);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToMacFarsi);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToMacGurmukhi);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToMacGujarati);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToMacHebrew);

static nsModuleComponentInfo components[] = 
{
  { 
    DECODER_NAME_BASE "us-ascii" , NS_ASCIITOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "us-ascii",
    nsAsciiToUnicodeConstructor ,
    nsAsciiToUnicodeRegSelf , nsAsciiToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "x-u-escaped" , NS_UESCAPETOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-u-escaped",
    nsUEscapeToUnicodeConstructor ,
    nsUEscapeToUnicodeRegSelf , nsUEscapeToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "ISO-8859-2" , NS_ISO88592TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-2",
    nsISO88592ToUnicodeConstructor ,
    nsISO88592ToUnicodeRegSelf , nsISO88592ToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "ISO-8859-3" , NS_ISO88593TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-3",
    nsISO88593ToUnicodeConstructor ,
    nsISO88593ToUnicodeRegSelf , nsISO88593ToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "ISO-8859-4" , NS_ISO88594TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-4",
    nsISO88594ToUnicodeConstructor ,
    nsISO88594ToUnicodeRegSelf , nsISO88594ToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "ISO-8859-5" , NS_ISO88595TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-5",
    nsISO88595ToUnicodeConstructor ,
    nsISO88595ToUnicodeRegSelf , nsISO88595ToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "ISO-8859-6" , NS_ISO88596TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-6",
    nsISO88596ToUnicodeConstructor ,
    nsISO88596ToUnicodeRegSelf , nsISO88596ToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "ISO-8859-6-I" , NS_ISO88596ITOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-6-I",
    nsISO88596IToUnicodeConstructor ,
    nsISO88596IToUnicodeRegSelf , nsISO88596IToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "ISO-8859-6-E" , NS_ISO88596ETOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-6-E",
    nsISO88596EToUnicodeConstructor ,
    nsISO88596EToUnicodeRegSelf , nsISO88596EToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "ISO-8859-7" , NS_ISO88597TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-7",
    nsISO88597ToUnicodeConstructor ,
    nsISO88597ToUnicodeRegSelf , nsISO88597ToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "ISO-8859-8" , NS_ISO88598TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-8",
    nsISO88598ToUnicodeConstructor ,
    nsISO88598ToUnicodeRegSelf , nsISO88598ToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "ISO-8859-8-I" , NS_ISO88598ITOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-8-I",
    nsISO88598IToUnicodeConstructor ,
    nsISO88598IToUnicodeRegSelf , nsISO88598IToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "ISO-8859-8-E" , NS_ISO88598ETOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-8-E",
    nsISO88598EToUnicodeConstructor ,
    nsISO88598EToUnicodeRegSelf , nsISO88598EToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "ISO-8859-9" , NS_ISO88599TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-9",
    nsISO88599ToUnicodeConstructor ,
    nsISO88599ToUnicodeRegSelf , nsISO88599ToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "ISO-8859-10" , NS_ISO885910TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-10",
    nsISO885910ToUnicodeConstructor ,
    nsISO885910ToUnicodeRegSelf , nsISO885910ToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "ISO-8859-13" , NS_ISO885913TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-13",
    nsISO885913ToUnicodeConstructor ,
    nsISO885913ToUnicodeRegSelf , nsISO885913ToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "ISO-8859-14" , NS_ISO885914TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-14",
    nsISO885914ToUnicodeConstructor ,
    nsISO885914ToUnicodeRegSelf , nsISO885914ToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "ISO-8859-15" , NS_ISO885915TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-15",
    nsISO885915ToUnicodeConstructor ,
    nsISO885915ToUnicodeRegSelf , nsISO885915ToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "ISO-IR-111" , NS_ISOIR111TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "ISO-IR-111",
    nsISOIR111ToUnicodeConstructor ,
    nsISOIR111ToUnicodeRegSelf , nsISOIR111ToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "windows-1250" , NS_CP1250TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "windows-1250",
    nsCP1250ToUnicodeConstructor ,
    nsCP1250ToUnicodeRegSelf , nsCP1250ToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "windows-1251" , NS_CP1251TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "windows-1251",
    nsCP1251ToUnicodeConstructor ,
    nsCP1251ToUnicodeRegSelf , nsCP1251ToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "windows-1253" , NS_CP1253TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "windows-1253",
    nsCP1253ToUnicodeConstructor ,
    nsCP1253ToUnicodeRegSelf , nsCP1253ToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "windows-1254" , NS_CP1254TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "windows-1254",
    nsCP1254ToUnicodeConstructor ,
    nsCP1254ToUnicodeRegSelf , nsCP1254ToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "windows-1255" , NS_CP1255TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "windows-1255",
    nsCP1255ToUnicodeConstructor ,
    nsCP1255ToUnicodeRegSelf , nsCP1255ToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "windows-1256" , NS_CP1256TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "windows-1256",
    nsCP1256ToUnicodeConstructor ,
    nsCP1256ToUnicodeRegSelf , nsCP1256ToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "windows-1257" , NS_CP1257TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "windows-1257",
    nsCP1257ToUnicodeConstructor ,
    nsCP1257ToUnicodeRegSelf , nsCP1257ToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "windows-1258" , NS_CP1258TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "windows-1258",
    nsCP1258ToUnicodeConstructor ,
    nsCP1258ToUnicodeRegSelf , nsCP1258ToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "TIS-620" , NS_CP874TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "TIS-620",
    nsCP874ToUnicodeConstructor ,
    nsCP874ToUnicodeRegSelf , nsCP874ToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "IBM866" , NS_CP866TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "IBM866",
    nsCP866ToUnicodeConstructor ,
    nsCP866ToUnicodeRegSelf , nsCP866ToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "KOI8-R" , NS_KOI8RTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "KOI8-R",
    nsKOI8RToUnicodeConstructor ,
    nsKOI8RToUnicodeRegSelf , nsKOI8RToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "KOI8-U" , NS_KOI8UTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "KOI8-U",
    nsKOI8UToUnicodeConstructor ,
    nsKOI8UToUnicodeRegSelf , nsKOI8UToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "x-mac-ce" , NS_MACCETOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-ce",
    nsMacCEToUnicodeConstructor ,
    nsMacCEToUnicodeRegSelf , nsMacCEToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "x-mac-greek" , NS_MACGREEKTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-greek",
    nsMacGreekToUnicodeConstructor ,
    nsMacGreekToUnicodeRegSelf , nsMacGreekToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "x-mac-turkish" , NS_MACTURKISHTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-turkish",
    nsMacTurkishToUnicodeConstructor ,
    nsMacTurkishToUnicodeRegSelf , nsMacTurkishToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "x-mac-croatian" , NS_MACCROATIANTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-croatian",
    nsMacCroatianToUnicodeConstructor ,
    nsMacCroatianToUnicodeRegSelf , nsMacCroatianToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "x-mac-romanian" , NS_MACROMANIANTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-romanian",
    nsMacRomanianToUnicodeConstructor ,
    nsMacRomanianToUnicodeRegSelf , nsMacRomanianToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "x-mac-cyrillic" , NS_MACCYRILLICTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-cyrillic",
    nsMacCyrillicToUnicodeConstructor ,
    nsMacCyrillicToUnicodeRegSelf , nsMacCyrillicToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "x-mac-ukrainian" , NS_MACUKRAINIANTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-ukrainian",
    nsMacUkrainianToUnicodeConstructor ,
    nsMacUkrainianToUnicodeRegSelf , nsMacUkrainianToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "x-mac-icelandic" , NS_MACICELANDICTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-icelandic",
    nsMacIcelandicToUnicodeConstructor ,
    nsMacIcelandicToUnicodeRegSelf , nsMacIcelandicToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "armscii-8" , NS_ARMSCII8TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "armscii-8",
    nsARMSCII8ToUnicodeConstructor ,
    nsARMSCII8ToUnicodeRegSelf , nsARMSCII8ToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "x-viet-tcvn5712" , NS_TCVN5712TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-viet-tcvn5712",
    nsTCVN5712ToUnicodeConstructor ,
    nsTCVN5712ToUnicodeRegSelf , nsTCVN5712ToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "VISCII" , NS_VISCIITOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "VISCII",
    nsVISCIIToUnicodeConstructor ,
    nsVISCIIToUnicodeRegSelf , nsVISCIIToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "x-viet-vps" , NS_VPSTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-viet-vps",
    nsVPSToUnicodeConstructor ,
    nsVPSToUnicodeRegSelf , nsVPSToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "UTF-7" , NS_UTF7TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "UTF-7",
    nsUTF7ToUnicodeConstructor ,
    nsUTF7ToUnicodeRegSelf , nsUTF7ToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "x-imap4-modified-utf7" , NS_MUTF7TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-imap4-modified-utf7",
    nsMUTF7ToUnicodeConstructor ,
    nsMUTF7ToUnicodeRegSelf , nsMUTF7ToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "UTF-16BE" , NS_UTF16BETOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "UTF-16BE",
    nsUTF16BEToUnicodeConstructor ,
    nsUTF16BEToUnicodeRegSelf , nsUTF16BEToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "UTF-16LE" , NS_UTF16LETOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "UTF-16LE",
    nsUTF16LEToUnicodeConstructor ,
    nsUTF16LEToUnicodeRegSelf , nsUTF16LEToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "UTF-32BE" , NS_UTF32BETOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "UTF-32BE",
    nsUCS4BEToUnicodeConstructor ,
    nsUCS4BEToUnicodeRegSelf , nsUCS4BEToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "UTF-32LE" , NS_UTF32LETOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "UTF-32LE",
    nsUCS4LEToUnicodeConstructor ,
    nsUCS4LEToUnicodeRegSelf , nsUCS4LEToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "T.61-8bit" , NS_T61TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "T.61-8bit",
    nsT61ToUnicodeConstructor ,
    nsT61ToUnicodeRegSelf , nsT61ToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "x-user-defined" , NS_USERDEFINEDTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-user-defined",
    nsUserDefinedToUnicodeConstructor ,
    nsUserDefinedToUnicodeRegSelf , nsUserDefinedToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "x-mac-arabic" , NS_MACARABICTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-arabic",
    nsMacArabicToUnicodeConstructor ,
    nsMacArabicToUnicodeRegSelf , nsMacArabicToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "x-mac-devanagari" , NS_MACDEVANAGARITOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-devanagari",
    nsMacDevanagariToUnicodeConstructor ,
    nsMacDevanagariToUnicodeRegSelf , nsMacDevanagariToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "x-mac-farsi" , NS_MACFARSITOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-farsi",
    nsMacFarsiToUnicodeConstructor ,
    nsMacFarsiToUnicodeRegSelf , nsMacFarsiToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "x-mac-gurmukhi" , NS_MACGURMUKHITOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-gurmukhi",
    nsMacGurmukhiToUnicodeConstructor ,
    nsMacGurmukhiToUnicodeRegSelf , nsMacGurmukhiToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "x-mac-gujarati" , NS_MACGUJARATITOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-gujarati",
    nsMacGujaratiToUnicodeConstructor ,
    nsMacGujaratiToUnicodeRegSelf , nsMacGujaratiToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "x-mac-hebrew" , NS_MACHEBREWTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-hebrew",
    nsMacHebrewToUnicodeConstructor ,
    nsMacHebrewToUnicodeRegSelf , nsMacHebrewToUnicodeUnRegSelf 
  },
  { 
    ENCODER_NAME_BASE "us-ascii" , NS_UNICODETOASCII_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "us-ascii",
    nsUnicodeToAsciiConstructor, 
    nsUnicodeToAsciiRegSelf, nsUnicodeToAsciiUnRegSelf
  },
  { 
    ENCODER_NAME_BASE "x-u-escaped" , NS_UNICODETOUESCAPE_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-u-escaped",
    nsUnicodeToUEscapeConstructor, 
    nsUnicodeToUEscapeRegSelf, nsUnicodeToUEscapeUnRegSelf
  },
  { 
    ENCODER_NAME_BASE "ISO-8859-2" , NS_UNICODETOISO88592_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-2",
    nsUnicodeToISO88592Constructor, 
    nsUnicodeToISO88592RegSelf, nsUnicodeToISO88592UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "ISO-8859-3" , NS_UNICODETOISO88593_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-3",
    nsUnicodeToISO88593Constructor, 
    nsUnicodeToISO88593RegSelf, nsUnicodeToISO88593UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "ISO-8859-4" , NS_UNICODETOISO88594_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-4",
    nsUnicodeToISO88594Constructor, 
    nsUnicodeToISO88594RegSelf, nsUnicodeToISO88594UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "ISO-8859-5" , NS_UNICODETOISO88595_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-5",
    nsUnicodeToISO88595Constructor, 
    nsUnicodeToISO88595RegSelf, nsUnicodeToISO88595UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "ISO-8859-6" , NS_UNICODETOISO88596_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-6",
    nsUnicodeToISO88596Constructor, 
    nsUnicodeToISO88596RegSelf, nsUnicodeToISO88596UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "ISO-8859-6-I" , NS_UNICODETOISO88596I_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-6-I",
    nsUnicodeToISO88596IConstructor, 
    nsUnicodeToISO88596IRegSelf, nsUnicodeToISO88596IUnRegSelf
  },
  { 
    ENCODER_NAME_BASE "ISO-8859-6-E" , NS_UNICODETOISO88596E_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-6-E",
    nsUnicodeToISO88596EConstructor, 
    nsUnicodeToISO88596ERegSelf, nsUnicodeToISO88596EUnRegSelf
  },
  { 
    ENCODER_NAME_BASE "ISO-8859-7" , NS_UNICODETOISO88597_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-7",
    nsUnicodeToISO88597Constructor, 
    nsUnicodeToISO88597RegSelf, nsUnicodeToISO88597UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "ISO-8859-8" , NS_UNICODETOISO88598_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-8",
    nsUnicodeToISO88598Constructor, 
    nsUnicodeToISO88598RegSelf, nsUnicodeToISO88598UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "ISO-8859-8-I" , NS_UNICODETOISO88598I_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-8-I",
    nsUnicodeToISO88598IConstructor, 
    nsUnicodeToISO88598IRegSelf, nsUnicodeToISO88598IUnRegSelf
  },
  { 
    ENCODER_NAME_BASE "ISO-8859-8-E" , NS_UNICODETOISO88598E_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-8-E",
    nsUnicodeToISO88598EConstructor, 
    nsUnicodeToISO88598ERegSelf, nsUnicodeToISO88598EUnRegSelf
  },
  { 
    ENCODER_NAME_BASE "ISO-8859-9" , NS_UNICODETOISO88599_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-9",
    nsUnicodeToISO88599Constructor, 
    nsUnicodeToISO88599RegSelf, nsUnicodeToISO88599UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "ISO-8859-10" , NS_UNICODETOISO885910_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-10",
    nsUnicodeToISO885910Constructor, 
    nsUnicodeToISO885910RegSelf, nsUnicodeToISO885910UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "ISO-8859-13" , NS_UNICODETOISO885913_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-13",
    nsUnicodeToISO885913Constructor, 
    nsUnicodeToISO885913RegSelf, nsUnicodeToISO885913UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "ISO-8859-14" , NS_UNICODETOISO885914_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-14",
    nsUnicodeToISO885914Constructor, 
    nsUnicodeToISO885914RegSelf, nsUnicodeToISO885914UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "ISO-8859-15" , NS_UNICODETOISO885915_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-15",
    nsUnicodeToISO885915Constructor, 
    nsUnicodeToISO885915RegSelf, nsUnicodeToISO885915UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "ISO-IR-111" , NS_UNICODETOISOIR111_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "ISO-IR-111",
    nsUnicodeToISOIR111Constructor, 
    nsUnicodeToISOIR111RegSelf, nsUnicodeToISOIR111UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "windows-1250" , NS_UNICODETOCP1250_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "windows-1250",
    nsUnicodeToCP1250Constructor, 
    nsUnicodeToCP1250RegSelf, nsUnicodeToCP1250UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "windows-1251" , NS_UNICODETOCP1251_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "windows-1251",
    nsUnicodeToCP1251Constructor, 
    nsUnicodeToCP1251RegSelf, nsUnicodeToCP1251UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "windows-1253" , NS_UNICODETOCP1253_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "windows-1253",
    nsUnicodeToCP1253Constructor, 
    nsUnicodeToCP1253RegSelf, nsUnicodeToCP1253UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "windows-1254" , NS_UNICODETOCP1254_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "windows-1254",
    nsUnicodeToCP1254Constructor, 
    nsUnicodeToCP1254RegSelf, nsUnicodeToCP1254UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "windows-1255" , NS_UNICODETOCP1255_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "windows-1255",
    nsUnicodeToCP1255Constructor, 
    nsUnicodeToCP1255RegSelf, nsUnicodeToCP1255UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "windows-1256" , NS_UNICODETOCP1256_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "windows-1256",
    nsUnicodeToCP1256Constructor, 
    nsUnicodeToCP1256RegSelf, nsUnicodeToCP1256UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "windows-1257" , NS_UNICODETOCP1257_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "windows-1257",
    nsUnicodeToCP1257Constructor, 
    nsUnicodeToCP1257RegSelf, nsUnicodeToCP1257UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "windows-1258" , NS_UNICODETOCP1258_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "windows-1258",
    nsUnicodeToCP1258Constructor, 
    nsUnicodeToCP1258RegSelf, nsUnicodeToCP1258UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "TIS-620" , NS_UNICODETOCP874_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "TIS-620",
    nsUnicodeToCP874Constructor, 
    nsUnicodeToCP874RegSelf, nsUnicodeToCP874UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "IBM866" , NS_UNICODETOCP866_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "IBM866",
    nsUnicodeToCP866Constructor, 
    nsUnicodeToCP866RegSelf, nsUnicodeToCP866UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "KOI8-R" , NS_UNICODETOKOI8R_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "KOI8-R",
    nsUnicodeToKOI8RConstructor, 
    nsUnicodeToKOI8RRegSelf, nsUnicodeToKOI8RUnRegSelf
  },
  { 
    ENCODER_NAME_BASE "KOI8-U" , NS_UNICODETOKOI8U_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "KOI8-U",
    nsUnicodeToKOI8UConstructor, 
    nsUnicodeToKOI8URegSelf, nsUnicodeToKOI8UUnRegSelf
  },
  { 
    ENCODER_NAME_BASE "x-mac-ce" , NS_UNICODETOMACCE_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-ce",
    nsUnicodeToMacCEConstructor, 
    nsUnicodeToMacCERegSelf, nsUnicodeToMacCEUnRegSelf
  },
  { 
    ENCODER_NAME_BASE "x-mac-greek" , NS_UNICODETOMACGREEK_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-greek",
    nsUnicodeToMacGreekConstructor, 
    nsUnicodeToMacGreekRegSelf, nsUnicodeToMacGreekUnRegSelf
  },
  { 
    ENCODER_NAME_BASE "x-mac-turkish" , NS_UNICODETOMACTURKISH_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-turkish",
    nsUnicodeToMacTurkishConstructor, 
    nsUnicodeToMacTurkishRegSelf, nsUnicodeToMacTurkishUnRegSelf
  },
  { 
    ENCODER_NAME_BASE "x-mac-croatian" , NS_UNICODETOMACCROATIAN_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-croatian",
    nsUnicodeToMacCroatianConstructor, 
    nsUnicodeToMacCroatianRegSelf, nsUnicodeToMacCroatianUnRegSelf
  },
  { 
    ENCODER_NAME_BASE "x-mac-romanian" , NS_UNICODETOMACROMANIAN_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-romanian",
    nsUnicodeToMacRomanianConstructor, 
    nsUnicodeToMacRomanianRegSelf, nsUnicodeToMacRomanianUnRegSelf
  },
  { 
    ENCODER_NAME_BASE "x-mac-cyrillic" , NS_UNICODETOMACCYRILLIC_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-cyrillic",
    nsUnicodeToMacCyrillicConstructor, 
    nsUnicodeToMacCyrillicRegSelf, nsUnicodeToMacCyrillicUnRegSelf
  },
  { 
    ENCODER_NAME_BASE "x-mac-ukrainian" , NS_UNICODETOMACUKRAINIAN_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-ukrainian",
    nsUnicodeToMacUkrainianConstructor, 
    nsUnicodeToMacUkrainianRegSelf, nsUnicodeToMacUkrainianUnRegSelf
  },
  { 
    ENCODER_NAME_BASE "x-mac-icelandic" , NS_UNICODETOMACICELANDIC_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-icelandic",
    nsUnicodeToMacIcelandicConstructor, 
    nsUnicodeToMacIcelandicRegSelf, nsUnicodeToMacIcelandicUnRegSelf
  },
  { 
    ENCODER_NAME_BASE "armscii-8" , NS_UNICODETOARMSCII8_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "armscii-8",
    nsUnicodeToARMSCII8Constructor, 
    nsUnicodeToARMSCII8RegSelf, nsUnicodeToARMSCII8UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "x-viet-tcvn5712" , NS_UNICODETOTCVN5712_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-viet-tcvn5712",
    nsUnicodeToTCVN5712Constructor, 
    nsUnicodeToTCVN5712RegSelf, nsUnicodeToTCVN5712UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "VISCII" , NS_UNICODETOVISCII_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "VISCII",
    nsUnicodeToVISCIIConstructor, 
    nsUnicodeToVISCIIRegSelf, nsUnicodeToVISCIIUnRegSelf
  },
  { 
    ENCODER_NAME_BASE "x-viet-vps" , NS_UNICODETOVPS_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-viet-vps",
    nsUnicodeToVPSConstructor, 
    nsUnicodeToVPSRegSelf, nsUnicodeToVPSUnRegSelf
  },
  { 
    ENCODER_NAME_BASE "UTF-7" , NS_UNICODETOUTF7_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "UTF-7",
    nsUnicodeToUTF7Constructor, 
    nsUnicodeToUTF7RegSelf, nsUnicodeToUTF7UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "x-imap4-modified-utf7" , NS_UNICODETOMUTF7_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-imap4-modified-utf7",
    nsUnicodeToMUTF7Constructor, 
    nsUnicodeToMUTF7RegSelf, nsUnicodeToMUTF7UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "UTF-16BE" , NS_UNICODETOUTF16BE_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "UTF-16BE",
    nsUnicodeToUTF16BEConstructor, 
    nsUnicodeToUTF16BERegSelf, nsUnicodeToUTF16BEUnRegSelf
  },
  { 
    ENCODER_NAME_BASE "UTF-16LE" , NS_UNICODETOUTF16LE_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "UTF-16LE",
    nsUnicodeToUTF16LEConstructor, 
    nsUnicodeToUTF16LERegSelf, nsUnicodeToUTF16LEUnRegSelf
  },
  { 
    ENCODER_NAME_BASE "UTF-16" , NS_UNICODETOUTF16_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "UTF-16",
    nsUnicodeToUTF16Constructor, 
    nsUnicodeToUTF16RegSelf, nsUnicodeToUTF16UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "UTF-32BE" , NS_UNICODETOUTF32BE_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "UTF-32BE",
    nsUnicodeToUCS4BEConstructor, 
    nsUnicodeToUCS4BERegSelf, nsUnicodeToUCS4BEUnRegSelf
  },
  { 
    ENCODER_NAME_BASE "UTF-32LE" , NS_UNICODETOUTF32LE_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "UTF-32LE",
    nsUnicodeToUCS4LEConstructor, 
    nsUnicodeToUCS4LERegSelf, nsUnicodeToUCS4LEUnRegSelf
  },
    { 
    ENCODER_NAME_BASE "T.61-8bit" , NS_UNICODETOT61_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "T.61-8bit",
    nsUnicodeToT61Constructor, 
    nsUnicodeToT61RegSelf, nsUnicodeToT61UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "x-user-defined" , NS_UNICODETOUSERDEFINED_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-user-defined",
    nsUnicodeToUserDefinedConstructor, 
    nsUnicodeToUserDefinedRegSelf, nsUnicodeToUserDefinedUnRegSelf
  },
  { 
    ENCODER_NAME_BASE "Adobe-Symbol-Encoding" , NS_UNICODETOSYMBOL_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "Adobe-Symbol-Encoding",
    nsUnicodeToSymbolConstructor, 
    nsUnicodeToSymbolRegSelf, nsUnicodeToSymbolUnRegSelf
  },
  { 
    ENCODER_NAME_BASE "x-zapf-dingbats" , NS_UNICODETOZAPFDINGBATS_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-zapf-dingbats",
    nsUnicodeToZapfDingbatConstructor, 
    nsUnicodeToZapfDingbatRegSelf, nsUnicodeToZapfDingbatUnRegSelf
  },
  { 
    ENCODER_NAME_BASE "x-mac-arabic" , NS_UNICODETOMACARABIC_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-arabic",
    nsUnicodeToMacArabicConstructor, 
    nsUnicodeToMacArabicRegSelf, nsUnicodeToMacArabicUnRegSelf
  },
  { 
    ENCODER_NAME_BASE "x-mac-devanagari" , NS_UNICODETOMACDEVANAGARI_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-devanagari",
    nsUnicodeToMacDevanagariConstructor, 
    nsUnicodeToMacDevanagariRegSelf, nsUnicodeToMacDevanagariUnRegSelf
  },
  { 
    ENCODER_NAME_BASE "x-mac-farsi" , NS_UNICODETOMACFARSI_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-farsi",
    nsUnicodeToMacFarsiConstructor, 
    nsUnicodeToMacFarsiRegSelf, nsUnicodeToMacFarsiUnRegSelf
  },
  { 
    ENCODER_NAME_BASE "x-mac-gurmukhi" , NS_UNICODETOMACGURMUKHI_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-gurmukhi",
    nsUnicodeToMacGurmukhiConstructor, 
    nsUnicodeToMacGurmukhiRegSelf, nsUnicodeToMacGurmukhiUnRegSelf
  },
  { 
    ENCODER_NAME_BASE "x-mac-gujarati" , NS_UNICODETOMACGUJARATI_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-gujarati",
    nsUnicodeToMacGujaratiConstructor, 
    nsUnicodeToMacGujaratiRegSelf, nsUnicodeToMacGujaratiUnRegSelf
  },
  { 
    ENCODER_NAME_BASE "x-mac-hebrew" , NS_UNICODETOMACHEBREW_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-hebrew",
    nsUnicodeToMacHebrewConstructor, 
    nsUnicodeToMacHebrewRegSelf, nsUnicodeToMacHebrewUnRegSelf
  }
};

NS_IMPL_NSGETMODULE(nsUCvLatinModule, components);

