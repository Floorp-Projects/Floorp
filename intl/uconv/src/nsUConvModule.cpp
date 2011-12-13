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
#include "mozilla/ModuleUtils.h"
#include "nsIComponentManager.h"
#include "nsICategoryManager.h"
#include "nsICharsetConverterManager.h"
#include "nsEncoderDecoderUtils.h"
#include "nsIUnicodeDecoder.h"
#include "nsIUnicodeEncoder.h"
#include "nsIServiceManager.h"


#include "nsUConvCID.h"
#include "nsCharsetConverterManager.h"
#include "nsTextToSubURI.h"
#include "nsUTF8ConverterService.h"
#include "nsConverterInputStream.h"
#include "nsConverterOutputStream.h"
#include "nsScriptableUConv.h"

#include "nsITextToSubURI.h"

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

NS_CONVERTER_REGISTRY_START
NS_UCONV_REG_UNREG("ISO-8859-1", NS_ISO88591TOUNICODE_CID, NS_UNICODETOISO88591_CID)
NS_UCONV_REG_UNREG("windows-1252", NS_CP1252TOUNICODE_CID, NS_UNICODETOCP1252_CID)
NS_UCONV_REG_UNREG("x-mac-roman", NS_MACROMANTOUNICODE_CID, NS_UNICODETOMACROMAN_CID)
NS_UCONV_REG_UNREG("UTF-8", NS_UTF8TOUNICODE_CID, NS_UNICODETOUTF8_CID)

{ NS_TITLE_BUNDLE_CATEGORY, "chrome://global/locale/charsetTitles.properties", "" },
{ NS_DATA_BUNDLE_CATEGORY, "resource://gre-resources/charsetData.properties", "" },
  
NS_CONVERTER_REGISTRY_END

NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToUTF8)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUTF8ToUnicode)


//----------------------------------------------------------------------------
// Global functions and data [declaration]

NS_GENERIC_FACTORY_CONSTRUCTOR(nsCharsetConverterManager)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTextToSubURI)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUTF8ConverterService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsConverterInputStream)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsConverterOutputStream)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsScriptableUnicodeConverter)

NS_DEFINE_NAMED_CID(NS_ICHARSETCONVERTERMANAGER_CID);
NS_DEFINE_NAMED_CID(NS_TEXTTOSUBURI_CID);
NS_DEFINE_NAMED_CID(NS_CONVERTERINPUTSTREAM_CID);
NS_DEFINE_NAMED_CID(NS_CONVERTEROUTPUTSTREAM_CID);
NS_DEFINE_NAMED_CID(NS_ISCRIPTABLEUNICODECONVERTER_CID);
NS_DEFINE_NAMED_CID(NS_UTF8CONVERTERSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_ISO88591TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_CP1252TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_MACROMANTOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_UTF8TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOISO88591_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOCP1252_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOMACROMAN_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOUTF8_CID);

static const mozilla::Module::CIDEntry kUConvCIDs[] = {
  { &kNS_ICHARSETCONVERTERMANAGER_CID, false, NULL, nsCharsetConverterManagerConstructor },
  { &kNS_TEXTTOSUBURI_CID, false, NULL, nsTextToSubURIConstructor },
  { &kNS_CONVERTERINPUTSTREAM_CID, false, NULL, nsConverterInputStreamConstructor },
  { &kNS_CONVERTEROUTPUTSTREAM_CID, false, NULL, nsConverterOutputStreamConstructor },
  { &kNS_ISCRIPTABLEUNICODECONVERTER_CID, false, NULL, nsScriptableUnicodeConverterConstructor },
  { &kNS_UTF8CONVERTERSERVICE_CID, false, NULL, nsUTF8ConverterServiceConstructor },
  { &kNS_ISO88591TOUNICODE_CID, false, NULL, nsISO88591ToUnicodeConstructor },
  { &kNS_CP1252TOUNICODE_CID, false, NULL, nsCP1252ToUnicodeConstructor },
  { &kNS_MACROMANTOUNICODE_CID, false, NULL, nsMacRomanToUnicodeConstructor },
  { &kNS_UTF8TOUNICODE_CID, false, NULL, nsUTF8ToUnicodeConstructor },
  { &kNS_UNICODETOISO88591_CID, false, NULL, nsUnicodeToISO88591Constructor },
  { &kNS_UNICODETOCP1252_CID, false, NULL, nsUnicodeToCP1252Constructor },
  { &kNS_UNICODETOMACROMAN_CID, false, NULL, nsUnicodeToMacRomanConstructor },
  { &kNS_UNICODETOUTF8_CID, false, NULL, nsUnicodeToUTF8Constructor },
  { NULL },
};

static const mozilla::Module::ContractIDEntry kUConvContracts[] = {
  { NS_CHARSETCONVERTERMANAGER_CONTRACTID, &kNS_ICHARSETCONVERTERMANAGER_CID },
  { NS_ITEXTTOSUBURI_CONTRACTID, &kNS_TEXTTOSUBURI_CID },
  { NS_CONVERTERINPUTSTREAM_CONTRACTID, &kNS_CONVERTERINPUTSTREAM_CID },
  { "@mozilla.org/intl/converter-output-stream;1", &kNS_CONVERTEROUTPUTSTREAM_CID },
  { NS_ISCRIPTABLEUNICODECONVERTER_CONTRACTID, &kNS_ISCRIPTABLEUNICODECONVERTER_CID },
  { NS_UTF8CONVERTERSERVICE_CONTRACTID, &kNS_UTF8CONVERTERSERVICE_CID },
  { NS_ISO88591TOUNICODE_CONTRACTID, &kNS_ISO88591TOUNICODE_CID },
  { NS_CP1252TOUNICODE_CONTRACTID, &kNS_CP1252TOUNICODE_CID },
  { NS_MACROMANTOUNICODE_CONTRACTID, &kNS_MACROMANTOUNICODE_CID },
  { NS_UTF8TOUNICODE_CONTRACTID, &kNS_UTF8TOUNICODE_CID },
  { NS_UNICODETOISO88591_CONTRACTID, &kNS_UNICODETOISO88591_CID },
  { NS_UNICODETOCP1252_CONTRACTID, &kNS_UNICODETOCP1252_CID },
  { NS_UNICODETOMACROMAN_CONTRACTID, &kNS_UNICODETOMACROMAN_CID },
  { NS_UNICODETOUTF8_CONTRACTID, &kNS_UNICODETOUTF8_CID },
  { NULL }
};

static const mozilla::Module kUConvModule = {
  mozilla::Module::kVersion,
  kUConvCIDs,
  kUConvContracts,
  kUConvCategories
};

NSMODULE_DEFN(nsUConvModule) = &kUConvModule;
