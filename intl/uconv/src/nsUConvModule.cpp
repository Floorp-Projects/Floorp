/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */
#include "nsCOMPtr.h"
#include "nsIRegistry.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsIComponentManager.h"
#include "nsICharsetConverterManager.h"
#include "nsIUnicodeDecodeHelper.h"
#include "nsIUnicodeEncodeHelper.h"
#include "nsIPlatformCharset.h"
#include "nsICharsetAlias.h"
#include "nsITextToSubURI.h"
#include "nsIServiceManager.h"
#include "nsCharsetMenu.h"
#include "rdf.h"
#include "nsUConvDll.h"
#include "nsFileSpec.h"
#include "nsIFile.h"
#include "nsIScriptableUConv.h"

#include "nsUCvMinSupport.h"
#include "nsISO88591ToUnicode.h"
#include "nsCP1252ToUnicode.h"
#include "nsMacRomanToUnicode.h"
#include "nsUTF8ToUnicode.h"
#include "nsUnicodeToISO88591.h"
#include "nsUnicodeToCP1252.h"
#include "nsUnicodeToMacRoman.h"
#include "nsUnicodeToUTF8.h"
#include "nsScriptableUConv.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

/**
 * Extra information about charset converters that is stored in the
 * component registry.
 */
struct ConverterInfo {
  nsCID       mCID;
  const char* mSource;
  const char* mDestination;
};

static ConverterInfo gConverterInfo[] = {
  { NS_ISO88591TOUNICODE_CID,  "ISO-8859-1",   "Unicode" },
  { NS_CP1252TOUNICODE_CID,    "windows-1252", "Unicode" },
  { NS_MACROMANTOUNICODE_CID,  "x-mac-roman",  "Unicode" },
  { NS_UTF8TOUNICODE_CID,      "UTF-8",        "Unicode" },
  { NS_UNICODETOISO88591_CID,  "Unicode",      "ISO-8859-1" },
  { NS_UNICODETOCP1252_CID,    "Unicode",      "windows-1252" },
  { NS_UNICODETOMACROMAN_CID,  "Unicode",      "x-mac-roman" },
  { NS_UNICODETOUTF8_CID,      "Unicode",      "UTF-8" },
};

#define NUM_CONVERTERS (sizeof(gConverterInfo) / sizeof(gConverterInfo[0]))

/**
 * Register the to/from information about a converter.
 */
static NS_IMETHODIMP
RegisterConverter(nsIComponentManager* aCompMgr,
                  nsIFile* aPath,
                  const char *aLocation,
                  const char *aType,
                  const nsModuleComponentInfo* aInfo)
{
  nsresult rv;

  // Find the relevant information for the converter we're registering
  const ConverterInfo* info = gConverterInfo;
  const ConverterInfo* limit = gConverterInfo + NUM_CONVERTERS;
  for ( ; info < limit; ++info) {
    if (info->mCID.Equals(aInfo->mCID))
      break;
  }

  if (info == limit) {
    NS_ERROR("failed to find converter info");
    return NS_OK; // so as not to completely kill component registration
  }

  nsCOMPtr<nsIRegistry> registry = do_GetService(NS_REGISTRY_CONTRACTID);
  if (! registry)
    return NS_ERROR_FAILURE;

  // Open the registry                                                          
  rv = registry->OpenWellKnownRegistry(nsIRegistry::ApplicationComponentRegistry);                               
  if (NS_FAILED(rv)) return rv;

  // Register source and destination strings under the key
  // ``software/netscape/intl/uconv/[cid]''.
  static NS_NAMED_LITERAL_CSTRING(kConverterKeyPrefix, "software/netscape/intl/uconv/");
  nsXPIDLCString cid;
  cid.Adopt(info->mCID.ToString());

  nsRegistryKey key;
  rv = registry->AddSubtree(nsIRegistry::Common,
                            PromiseFlatCString(kConverterKeyPrefix + cid).get(),
                            &key);

  if (NS_SUCCEEDED(rv)) {
    registry->SetStringUTF8(key, "source", info->mSource);
    registry->SetStringUTF8(key, "destination", info->mDestination);
  }

  return NS_OK;
}

/**
 * Unregister the extra converter information.
 */
static NS_IMETHODIMP
UnregisterConverter(nsIComponentManager* aCompMgr,
                    nsIFile* aPath,
                    const char* aRegistryLocation,
                    const nsModuleComponentInfo* aInfo)
{
  // XXX TODO: remove converter key
  return NS_OK;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(nsScriptableUnicodeConverter)

// The list of components we register
static nsModuleComponentInfo gComponents[] = {
  { "Charset Conversion Manager", NS_ICHARSETCONVERTERMANAGER_CID,
    NS_CHARSETCONVERTERMANAGER_CONTRACTID, NS_NewCharsetConverterManager,
    NS_RegisterConverterManagerData,
    NS_UnregisterConverterManagerData },

  { "Unicode Decode Helper", NS_UNICODEDECODEHELPER_CID,
    NS_UNICODEDECODEHELPER_CONTRACTID, NS_NewUnicodeDecodeHelper },

  { "Unicode Encode Helper", NS_UNICODEENCODEHELPER_CID,
    NS_UNICODEENCODEHELPER_CONTRACTID, NS_NewUnicodeEncodeHelper },
  
  { "Platform Charset Information", NS_PLATFORMCHARSET_CID,
    NS_PLATFORMCHARSET_CONTRACTID, NS_NewPlatformCharset },

  { "Charset Alias Information",  NS_CHARSETALIAS_CID,
    NS_CHARSETALIAS_CONTRACTID, NS_NewCharsetAlias },

  { NS_CHARSETMENU_PID, NS_CHARSETMENU_CID,
    NS_RDF_DATASOURCE_CONTRACTID_PREFIX NS_CHARSETMENU_PID,
    NS_NewCharsetMenu},

  { "Text To Sub URI Helper", NS_TEXTTOSUBURI_CID,
    NS_ITEXTTOSUBURI_CONTRACTID, NS_NewTextToSubURI },

  { "Unicode Encoder / Decoder for Script", NS_ISCRIPTABLEUNICODECONVERTER_CID,
    NS_ISCRIPTABLEUNICODECONVERTER_CONTRACTID, nsScriptableUnicodeConverterConstructor },

  // Converters
  { "ISO-8859-1 To Unicode Converter", NS_ISO88591TOUNICODE_CID,
    NS_ISO88591TOUNICODE_CONTRACTID, NS_NewISO88591ToUnicode,
    RegisterConverter, UnregisterConverter },

  { "windows-1252 To Unicode Converter", NS_CP1252TOUNICODE_CID,
    NS_CP1252TOUNICODE_CONTRACTID, NS_NewCP1252ToUnicode,
    RegisterConverter, UnregisterConverter },

  { "x-mac-roman To Unicode Converter", NS_MACROMANTOUNICODE_CID,
    NS_MACROMANTOUNICODE_CONTRACTID, NS_NewMacRomanToUnicode,
    RegisterConverter, UnregisterConverter },

  { "UTF-8 To Unicode Converter", NS_UTF8TOUNICODE_CID,
    NS_UTF8TOUNICODE_CONTRACTID, NS_NewUTF8ToUnicode,
    RegisterConverter, UnregisterConverter },

  { "Unicode To ISO-8859-1 Converter", NS_UNICODETOISO88591_CID,
    NS_UNICODETOISO88591_CONTRACTID, NS_NewUnicodeToISO88591,
    RegisterConverter, UnregisterConverter },

  { "Unicode To windows-1252 Converter", NS_UNICODETOCP1252_CID,
    NS_UNICODETOCP1252_CONTRACTID, NS_NewUnicodeToCP1252,
    RegisterConverter, UnregisterConverter },

  { "Unicode To x-mac-roman Converter", NS_UNICODETOMACROMAN_CID,
    NS_UNICODETOMACROMAN_CONTRACTID, NS_NewUnicodeToMacRoman,
    RegisterConverter, UnregisterConverter },

  { "Unicode To UTF-8 Converter", NS_UNICODETOUTF8_CID,
    NS_UNICODETOUTF8_CONTRACTID, NS_NewUnicodeToUTF8,
    RegisterConverter, UnregisterConverter },
};

NS_IMPL_NSGETMODULE(nsUConvModule, gComponents)
