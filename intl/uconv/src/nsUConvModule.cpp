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
#include "nsCOMPtr.h"
#include "nsIRegistry.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsIComponentManager.h"
#include "nsICharsetConverterManager.h"
#include "nsICharsetConverterManager2.h"
#include "nsIUnicodeDecodeHelper.h"
#include "nsIUnicodeEncodeHelper.h"
#include "nsIPlatformCharset.h"
#include "nsICharsetAlias.h"
#include "nsITextToSubURI.h"
#include "nsIServiceManager.h"
#include "nsUConvDll.h"
#include "nsFileSpec.h"
#include "nsIFile.h"
#include "nsIScriptableUConv.h"
#include "nsConverterInputStream.h"

#include "nsUCvMinSupport.h"
#include "nsCharsetConverterManager.h"
#include "nsUnicodeDecodeHelper.h"
#include "nsUnicodeEncodeHelper.h"
#include "nsPlatformCharset.h"
#include "nsCharsetAlias.h"
#include "nsTextToSubURI.h"
#include "nsISO88591ToUnicode.h"
#include "nsCP1252ToUnicode.h"
#include "nsMacRomanToUnicode.h"
#include "nsUTF8ToUnicode.h"
#include "nsUnicodeToISO88591.h"
#include "nsUnicodeToCP1252.h"
#include "nsUnicodeToMacRoman.h"
#include "nsUnicodeToUTF8.h"
#include "nsScriptableUConv.h"

NS_CONVERTER_REGISTRY_START
NS_UCONV_REG_UNREG("ISO-8859-1", "Unicode", NS_ISO88591TOUNICODE_CID)
NS_UCONV_REG_UNREG("windows-1252", "Unicode", NS_CP1252TOUNICODE_CID)
NS_UCONV_REG_UNREG("x-mac-roman", "Unicode", NS_MACROMANTOUNICODE_CID)
NS_UCONV_REG_UNREG("UTF-8", "Unicode", NS_UTF8TOUNICODE_CID)
NS_UCONV_REG_UNREG("Unicode", "ISO-8859-1", NS_UNICODETOISO88591_CID)
NS_UCONV_REG_UNREG("Unicode", "windows-1252",  NS_UNICODETOCP1252_CID)
NS_UCONV_REG_UNREG("Unicode", "x-mac-roman", NS_UNICODETOMACROMAN_CID)
NS_UCONV_REG_UNREG("Unicode", "UTF-8",  NS_UNICODETOUTF8_CID)
NS_CONVERTER_REGISTRY_END

NS_IMPL_NSUCONVERTERREGSELF

NS_GENERIC_FACTORY_CONSTRUCTOR(nsCharsetConverterManager)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeDecodeHelper)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeEncodeHelper)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPlatformCharset, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCharsetAlias2)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTextToSubURI)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsScriptableUnicodeConverter)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsConverterInputStream)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsISO88591ToUnicode)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCP1252ToUnicode)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMacRomanToUnicode)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUTF8ToUnicode)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToISO88591)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToCP1252)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToMacRoman)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToUTF8)

NS_IMETHODIMP
nsConverterManagerDataRegister(nsIComponentManager* aCompMgr,
                                nsIFile* aPath,
                                const char *aLocation,
                                const char *aType,
                                const nsModuleComponentInfo* aInfo)
{
  return nsCharsetConverterManager::RegisterConverterManagerData();
}

static const nsModuleComponentInfo components[] = 
{
  { 
    "Charset Conversion Manager", NS_ICHARSETCONVERTERMANAGER_CID,
    NS_CHARSETCONVERTERMANAGER_CONTRACTID, 
    nsCharsetConverterManagerConstructor,
    nsConverterManagerDataRegister,
  },
  { 
    "Unicode Decode Helper", NS_UNICODEDECODEHELPER_CID,
    NS_UNICODEDECODEHELPER_CONTRACTID, 
    nsUnicodeDecodeHelperConstructor 
  },
  { 
    "Unicode Encode Helper", NS_UNICODEENCODEHELPER_CID,
    NS_UNICODEENCODEHELPER_CONTRACTID, 
    nsUnicodeEncodeHelperConstructor 
  },
  { 
    "Platform Charset Information", NS_PLATFORMCHARSET_CID,
    NS_PLATFORMCHARSET_CONTRACTID, 
    nsPlatformCharsetConstructor
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
    "Unicode Encoder / Decoder for Script", NS_ISCRIPTABLEUNICODECONVERTER_CID,
    NS_ISCRIPTABLEUNICODECONVERTER_CONTRACTID, 
    nsScriptableUnicodeConverterConstructor
  },
  { "Unicode converter input stream", NS_CONVERTERINPUTSTREAM_CID,              
    NS_CONVERTERINPUTSTREAM_CONTRACTID, 
    nsConverterInputStreamConstructor 
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
  }
};

NS_IMPL_NSGETMODULE(nsUConvModule, components);

