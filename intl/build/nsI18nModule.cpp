/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsIGenericFactory.h"
#include "nsCOMPtr.h"
#include "nsICategoryManager.h"
#include "nsIServiceManager.h"

// chardet
#include "nsCharDetConstructors.h"

// lwbrk
#include "nsLWBrkConstructors.h"
#include "nsSemanticUnitScanner.h"

// unicharutil
#include "nsUcharUtilConstructors.h"

// string bundles (intl)
#include "nsStrBundleConstructors.h"

// locale
#include "nsLocaleConstructors.h"


NS_GENERIC_FACTORY_CONSTRUCTOR(nsSemanticUnitScanner)

static NS_METHOD
AddCategoryEntry(const char* category,
                 const char* key,
                 const char* value)
{
  nsresult rv;
  nsCOMPtr<nsICategoryManager> 
    categoryManager(do_GetService("@mozilla.org/categorymanager;1", &rv));
  if (NS_FAILED(rv)) return rv;
  
  return categoryManager->AddCategoryEntry(category, key, value, 
                                           PR_TRUE, PR_TRUE,
                                           nsnull);
}

static NS_METHOD
DeleteCategoryEntry(const char* category,
                    const char* key)
{
  nsresult rv;
  nsCOMPtr<nsICategoryManager> 
    categoryManager(do_GetService("@mozilla.org/categorymanager;1", &rv));
  if (NS_FAILED(rv)) return rv;
  
  return categoryManager->DeleteCategoryEntry(category, key, PR_TRUE);
}

static NS_METHOD
nsMetaCharsetObserverRegistrationProc(nsIComponentManager *aCompMgr,
                                      nsIFile *aPath,
                                      const char *registryLocation,
                                      const char *componentType,
                                      const nsModuleComponentInfo *info)
{
  return AddCategoryEntry("parser-service-category", 
                          "Meta Charset Service",
                          NS_META_CHARSET_CONTRACTID);
}

static NS_METHOD
nsMetaCharsetObserverUnegistrationProc(nsIComponentManager *aCompMgr,
                                       nsIFile *aPath,
                                       const char *registryLocation,
                                       const nsModuleComponentInfo *info)
{
  return DeleteCategoryEntry("parser-service-category",
                             "Meta Charset Service");
}

static NS_METHOD
nsDetectionAdaptorRegistrationProc(nsIComponentManager *aCompMgr,
                                   nsIFile *aPath,
                                   const char *registryLocation,
                                   const char *componentType,
                                   const nsModuleComponentInfo *info)
{
  return AddCategoryEntry(NS_CHARSET_DETECTOR_CATEGORY, "off", "off");
}

static NS_METHOD
nsJAPSMDetectorRegistrationProc(nsIComponentManager *aCompMgr,
                                nsIFile *aPath,
                                const char *registryLocation,
                                const char *componentType,
                                const nsModuleComponentInfo *info)
{
  return AddCategoryEntry(NS_CHARSET_DETECTOR_CATEGORY,
                          "ja_parallel_state_machine",
                          info->mContractID);
}

static NS_METHOD
nsKOPSMDetectorRegistrationProc(nsIComponentManager *aCompMgr,
                                nsIFile *aPath,
                                const char *registryLocation,
                                const char *componentType,
                                const nsModuleComponentInfo *info)
{
  return AddCategoryEntry(NS_CHARSET_DETECTOR_CATEGORY,
                          "ko_parallel_state_machine",
                          info->mContractID);
}

static NS_METHOD
nsZHTWPSMDetectorRegistrationProc(nsIComponentManager *aCompMgr,
                                  nsIFile *aPath,
                                  const char *registryLocation,
                                  const char *componentType,
                                  const nsModuleComponentInfo *info)
{
  return AddCategoryEntry(NS_CHARSET_DETECTOR_CATEGORY,
                          "zhtw_parallel_state_machine",
                          info->mContractID);
}

static NS_METHOD
nsZHCNPSMDetectorRegistrationProc(nsIComponentManager *aCompMgr,
                                  nsIFile *aPath,
                                  const char *registryLocation,
                                  const char *componentType,
                                  const nsModuleComponentInfo *info)
{
  return AddCategoryEntry(NS_CHARSET_DETECTOR_CATEGORY,
                          "zhcn_parallel_state_machine",
                          info->mContractID);
}

static NS_METHOD
nsZHPSMDetectorRegistrationProc(nsIComponentManager *aCompMgr,
                                nsIFile *aPath,
                                const char *registryLocation,
                                const char *componentType,
                                const nsModuleComponentInfo *info)
{
  return AddCategoryEntry(NS_CHARSET_DETECTOR_CATEGORY,
                          "zh_parallel_state_machine",
                          info->mContractID);
}

static NS_METHOD
nsCJKPSMDetectorRegistrationProc(nsIComponentManager *aCompMgr,
                                 nsIFile *aPath,
                                 const char *registryLocation,
                                 const char *componentType,
                                 const nsModuleComponentInfo *info)
{
  return AddCategoryEntry(NS_CHARSET_DETECTOR_CATEGORY,
                          "cjk_parallel_state_machine",
                          info->mContractID);
}

static NS_METHOD
nsRUProbDetectorRegistrationProc(nsIComponentManager *aCompMgr,
                                 nsIFile *aPath,
                                 const char *registryLocation,
                                 const char *componentType,
                                 const nsModuleComponentInfo *info)
{
  return AddCategoryEntry(NS_CHARSET_DETECTOR_CATEGORY,
                          "ruprob",
                          info->mContractID);
}

static NS_METHOD
nsUKProbDetectorRegistrationProc(nsIComponentManager *aCompMgr,
                                 nsIFile *aPath,
                                 const char *registryLocation,
                                 const char *componentType,
                                 const nsModuleComponentInfo *info)
{
  return AddCategoryEntry(NS_CHARSET_DETECTOR_CATEGORY,
                          "ukprob",
                          info->mContractID);
}


static nsModuleComponentInfo components[] =
{
    // chardet
 { "Meta Charset", NS_META_CHARSET_CID, 
    NS_META_CHARSET_CONTRACTID, nsMetaCharsetObserverConstructor, 
    nsMetaCharsetObserverRegistrationProc, nsMetaCharsetObserverUnegistrationProc,
    NULL},
 { "Document Charset Info", NS_DOCUMENTCHARSETINFO_CID, 
    NS_DOCUMENTCHARSETINFO_CONTRACTID, nsDocumentCharsetInfoConstructor, 
    NULL, NULL},
 { "XML Encoding", NS_XML_ENCODING_CID, 
    NS_XML_ENCODING_CONTRACTID, nsXMLEncodingObserverConstructor, 
    NULL, NULL},
 { "Charset Detection Adaptor", NS_CHARSET_DETECTION_ADAPTOR_CID, 
    NS_CHARSET_DETECTION_ADAPTOR_CONTRACTID, nsDetectionAdaptorConstructor, 
    nsDetectionAdaptorRegistrationProc, NULL},
 { "PSM based Japanese Charset Detector", NS_JA_PSMDETECTOR_CID, 
    NS_CHARSET_DETECTOR_CONTRACTID_BASE "ja_parallel_state_machine", nsJAPSMDetectorConstructor, 
    nsJAPSMDetectorRegistrationProc, NULL},
 { "PSM based Japanese String Charset Detector", NS_JA_STRING_PSMDETECTOR_CID, 
    NS_STRCDETECTOR_CONTRACTID_BASE "ja_parallel_state_machine", nsJAStringPSMDetectorConstructor, 
    NULL, NULL},
 { "PSM based Korean Charset Detector", NS_KO_PSMDETECTOR_CID, 
    NS_CHARSET_DETECTOR_CONTRACTID_BASE "ko_parallel_state_machine", nsKOPSMDetectorConstructor, 
    nsKOPSMDetectorRegistrationProc, NULL},
 { "PSM based Korean String Charset Detector", NS_KO_STRING_PSMDETECTOR_CID, 
    NS_STRCDETECTOR_CONTRACTID_BASE "ko_parallel_state_machine", nsKOStringPSMDetectorConstructor, 
    NULL, NULL},
 { "PSM based Traditional Chinese Charset Detector", NS_ZHTW_PSMDETECTOR_CID, 
    NS_CHARSET_DETECTOR_CONTRACTID_BASE "zhtw_parallel_state_machine", nsZHTWPSMDetectorConstructor, 
    nsZHTWPSMDetectorRegistrationProc, NULL},
 { "PSM based Traditional Chinese String Charset Detector", NS_ZHTW_STRING_PSMDETECTOR_CID, 
    NS_STRCDETECTOR_CONTRACTID_BASE "zhtw_parallel_state_machine", nsZHTWStringPSMDetectorConstructor, 
    NULL, NULL},
 { "PSM based Simplified Chinese Charset Detector", NS_ZHCN_PSMDETECTOR_CID, 
    NS_CHARSET_DETECTOR_CONTRACTID_BASE "zhcn_parallel_state_machine", nsZHCNPSMDetectorConstructor, 
    nsZHCNPSMDetectorRegistrationProc, NULL},
 { "PSM based Simplified Chinese String Charset Detector", NS_ZHCN_STRING_PSMDETECTOR_CID, 
    NS_STRCDETECTOR_CONTRACTID_BASE "zhcn_parallel_state_machine", nsZHCNStringPSMDetectorConstructor, 
    NULL, NULL},
 { "PSM based Chinese Charset Detector", NS_ZH_PSMDETECTOR_CID, 
    NS_CHARSET_DETECTOR_CONTRACTID_BASE "zh_parallel_state_machine", nsZHPSMDetectorConstructor, 
    nsZHPSMDetectorRegistrationProc, NULL},
 { "PSM based Chinese String Charset Detector", NS_ZH_STRING_PSMDETECTOR_CID, 
    NS_STRCDETECTOR_CONTRACTID_BASE "zh_parallel_state_machine", nsZHStringPSMDetectorConstructor, 
    NULL, NULL},
 { "PSM based CJK Charset Detector", NS_CJK_PSMDETECTOR_CID, 
    NS_CHARSET_DETECTOR_CONTRACTID_BASE "cjk_parallel_state_machine", nsCJKPSMDetectorConstructor, 
    nsCJKPSMDetectorRegistrationProc, NULL},
 { "PSM based CJK String Charset Detector", NS_CJK_STRING_PSMDETECTOR_CID, 
    NS_STRCDETECTOR_CONTRACTID_BASE "cjk_parallel_state_machine", nsCJKStringPSMDetectorConstructor, 
    NULL, NULL},
 { "Probability based Russian Charset Detector", NS_RU_PROBDETECTOR_CID, 
    NS_CHARSET_DETECTOR_CONTRACTID_BASE "ruprob", nsRUProbDetectorConstructor, 
    nsRUProbDetectorRegistrationProc, NULL},
 { "Probability based Ukrainian Charset Detector", NS_UK_PROBDETECTOR_CID, 
    NS_CHARSET_DETECTOR_CONTRACTID_BASE "ukprob", nsUKProbDetectorConstructor, 
    nsUKProbDetectorRegistrationProc, NULL},
 { "Probability based Russian String Charset Detector", NS_RU_STRING_PROBDETECTOR_CID, 
    NS_STRCDETECTOR_CONTRACTID_BASE "ruprob", nsRUStringProbDetectorConstructor, 
    NULL, NULL},
 { "Probability based Ukrainian String Charset Detector", NS_UK_STRING_PROBDETECTOR_CID, 
    NS_STRCDETECTOR_CONTRACTID_BASE "ukprob", nsUKStringProbDetectorConstructor, 
   NULL, NULL},
#ifdef INCLUDE_DBGDETECTOR
 { "Debugging Detector 1st block", NS_1STBLKDBG_DETECTOR_CID, 
    NS_CHARSET_DETECTOR_CONTRACTID_BASE "1stblkdbg", ns1stBlkDbgDetectorConstructor, 
    NULL, NULL},
 { "Debugging Detector 2nd block", NS_2NDBLKDBG_DETECTOR_CID, 
    NS_CHARSET_DETECTOR_CONTRACTID_BASE "2ndblkdbg", ns2ndBlkDbgDetectorConstructor, 
    NULL, NULL},
 { "Debugging Detector Last block", NS_LASTBLKDBG_DETECTOR_CID, 
    NS_CHARSET_DETECTOR_CONTRACTID_BASE "lastblkdbg", nsLastBlkDbgDetectorConstructor, 
    NULL, NULL},
#endif /* INCLUDE_DBGDETECTOR */
 // lwbrk
  { "Line and Word Breaker", NS_LWBRK_CID,
    NS_LWBRK_CONTRACTID, nsLWBreakerFImpConstructor},
  { "Semantic Unit Scanner", NS_SEMANTICUNITSCANNER_CID,
    NS_SEMANTICUNITSCANNER_CONTRACTID, nsSemanticUnitScannerConstructor},

 // unicharutil
  { "Unichar Utility", NS_UNICHARUTIL_CID, 
      NS_UNICHARUTIL_CONTRACTID, nsCaseConversionImp2Constructor},
  { "Unicode To Entity Converter", NS_ENTITYCONVERTER_CID, 
      NS_ENTITYCONVERTER_CONTRACTID, nsEntityConverterConstructor },
  { "Unicode To Charset Converter", NS_SAVEASCHARSET_CID, 
      NS_SAVEASCHARSET_CONTRACTID, nsSaveAsCharsetConstructor},
  { "Japanese Hankaku To Zenkaku", NS_HANKAKUTOZENKAKU_CID, 
      NS_HANKAKUTOZENKAKU_CONTRACTID, CreateNewHankakuToZenkaku},
  { "Unicode Normlization", NS_UNICODE_NORMALIZER_CID, 
      NS_UNICODE_NORMALIZER_CONTRACTID,  nsUnicodeNormalizerConstructor},


 // strres
  { "String Bundle", NS_STRINGBUNDLESERVICE_CID, NS_STRINGBUNDLE_CONTRACTID,
    nsStringBundleServiceConstructor},
  { "String Textfile Overrides", NS_STRINGBUNDLETEXTOVERRIDE_CID,
    NS_STRINGBUNDLETEXTOVERRIDE_CONTRACTID,
    nsStringBundleTextOverrideConstructor },

 // locale
  { "nsLocaleService component",
    NS_LOCALESERVICE_CID,
    NS_LOCALESERVICE_CONTRACTID,
    CreateLocaleService },
  { "Collation factory",
    NS_COLLATIONFACTORY_CID,
    NS_COLLATIONFACTORY_CONTRACTID,
    nsCollationFactoryConstructor },
  { "Scriptable Date Format",
    NS_SCRIPTABLEDATEFORMAT_CID,
    NS_SCRIPTABLEDATEFORMAT_CONTRACTID,
    NS_NewScriptableDateFormat },
  { "Language Atom Service",
    NS_LANGUAGEATOMSERVICE_CID,
    NS_LANGUAGEATOMSERVICE_CONTRACTID,
    nsLanguageAtomServiceConstructor },
  { "Font Package Service",
    NS_FONTPACKAGESERVICE_CID,
    NS_FONTPACKAGESERVICE_CONTRACTID,
    nsFontPackageServiceConstructor },
 
#ifdef XP_WIN 
  { "Platform locale",
    NS_WIN32LOCALE_CID,
    NS_WIN32LOCALE_CONTRACTID,
    nsIWin32LocaleImplConstructor },
  { "Collation",
    NS_COLLATION_CID,
    NS_COLLATION_CONTRACTID,
    nsCollationWinConstructor },
  { "Date/Time formatter",
    NS_DATETIMEFORMAT_CID,
    NULL,
    nsDateTimeFormatWinConstructor },
#endif
 
#ifdef USE_UNIX_LOCALE
  { "Platform locale",
    NS_POSIXLOCALE_CID,
    NS_POSIXLOCALE_CONTRACTID,
    nsPosixLocaleConstructor },

  { "Collation",
    NS_COLLATION_CID,
    NS_COLLATION_CONTRACTID,
    nsCollationUnixConstructor },

  { "Date/Time formatter",
    NS_DATETIMEFORMAT_CID,
    NULL,
    nsDateTimeFormatUnixConstructor },
#endif

#ifdef USE_MAC_LOCALE
  { "Mac locale",
    NS_MACLOCALE_CID,
    NS_MACLOCALE_CONTRACTID,
    nsMacLocaleConstructor },
  { "Collation",
    NS_COLLATION_CID,
    NS_COLLATION_CONTRACTID,
#ifdef USE_UCCOLLATIONKEY
    nsCollationMacUCConstructor },
#else
    nsCollationMacConstructor },
#endif
  { "Date/Time formatter",
    NS_DATETIMEFORMAT_CID,
    NULL,
    nsDateTimeFormatMacConstructor },
#endif

#ifdef XP_OS2
  { "OS/2 locale",
    NS_OS2LOCALE_CID,
    NS_OS2LOCALE_CONTRACTID,
    nsOS2LocaleConstructor },
  { "Collation",
    NS_COLLATION_CID,
    NS_COLLATION_CONTRACTID,
    nsCollationOS2Constructor },
  { "Date/Time formatter",
    NS_DATETIMEFORMAT_CID,
    NULL,
    nsDateTimeFormatOS2Constructor },
#endif
      
};


NS_IMPL_NSGETMODULE(nsI18nModule, components)
