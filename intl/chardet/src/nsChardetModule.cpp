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

#include "nsCharDetConstructors.h"


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


static const nsModuleComponentInfo components[] =
{
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
};


NS_IMPL_NSGETMODULE(nsChardetModule, components)
