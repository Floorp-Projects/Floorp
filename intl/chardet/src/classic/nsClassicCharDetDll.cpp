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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#define NS_IMPL_IDS
#include "nsICharsetAlias.h"
#undef NS_IMPL_IDS

#include "pratom.h"
#include "nsClassicCharDetDll.h"
#include "nsICharsetDetectionObserver.h"
#include "nsISupports.h"
#include "nsIComponentManager.h"
#include "nsIFactory.h"
#include "nsIServiceManager.h"
#include "nsIGenericFactory.h"
#include "nsCOMPtr.h"
#include "nsICharsetDetector.h"
#include "nsIStringCharsetDetector.h"
#include "nsClassicDetectors.h"


static NS_DEFINE_CID(kJAClassicDetectorCID,         NS_JA_CLASSIC_DETECTOR_CID);
static NS_DEFINE_CID(kJAClassicStringDetectorCID,   NS_JA_CLASSIC_STRING_DETECTOR_CID);
static NS_DEFINE_CID(kKOClassicDetectorCID,         NS_KO_CLASSIC_DETECTOR_CID);
static NS_DEFINE_CID(kKOClassicStringDetectorCID,   NS_KO_CLASSIC_STRING_DETECTOR_CID);

NS_GENERIC_FACTORY_CONSTRUCTOR(nsJACharsetClassicDetector);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsJAStringCharsetClassicDetector);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsKOCharsetClassicDetector);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsKOStringCharsetClassicDetector);

static nsModuleComponentInfo components[] = {
  { "Classic JA Charset Detector", NS_JA_CLASSIC_DETECTOR_CID,
     NS_CHARSET_DETECTOR_CONTRACTID_BASE "jaclassic", nsJACharsetClassicDetectorConstructor},
  { "Classic JA String Charset Detector", NS_JA_CLASSIC_DETECTOR_CID,
     NS_STRCDETECTOR_CONTRACTID_BASE "jaclassic", nsJAStringCharsetClassicDetectorConstructor},
  { "Classic KO Charset Detector", NS_KO_CLASSIC_DETECTOR_CID,
     NS_CHARSET_DETECTOR_CONTRACTID_BASE "koclassic", nsKOCharsetClassicDetectorConstructor},
  { "Classic KO String Charset Detector", NS_KO_CLASSIC_STRING_DETECTOR_CID,
     NS_STRCDETECTOR_CONTRACTID_BASE "koclassic", nsKOStringCharsetClassicDetectorConstructor}
};

NS_IMPL_NSGETMODULE(nsCharDetModuleClassic, components);

