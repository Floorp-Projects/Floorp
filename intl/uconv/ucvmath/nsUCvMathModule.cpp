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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
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
#include "nsUCvMathCID.h"
#include "nsUCvMathDll.h"

#if defined(XP_PC) || defined(XP_MAC)
#include "nsUnicodeToTeXCMRttf.h"
#include "nsUnicodeToTeXCMMIttf.h"
#include "nsUnicodeToTeXCMSYttf.h"
#include "nsUnicodeToTeXCMEXttf.h"
#else
#include "nsUnicodeToTeXCMRt1.h"
#include "nsUnicodeToTeXCMMIt1.h"
#include "nsUnicodeToTeXCMSYt1.h"
#include "nsUnicodeToTeXCMEXt1.h"
#endif
#include "nsUnicodeToMathematica1.h"
#include "nsUnicodeToMathematica2.h"
#include "nsUnicodeToMathematica3.h"
#include "nsUnicodeToMathematica4.h"
#include "nsUnicodeToMathematica5.h"
#include "nsUnicodeToMTExtra.h"

//----------------------------------------------------------------------------
// Global functions and data [declaration]

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

#define DECODER_NAME_BASE "Unicode Decoder-"
#define ENCODER_NAME_BASE "Unicode Encoder-"

NS_IMPL_NSUCONVERTERREGSELF

#if defined(XP_PC) || defined(XP_MAC)
NS_UCONV_REG_UNREG(nsUnicodeToTeXCMRttf,  "Unicode", "x-ttf-cmr",   NS_UNICODETOTEXCMRTTF_CID);
NS_UCONV_REG_UNREG(nsUnicodeToTeXCMMIttf, "Unicode", "x-ttf-cmmi",  NS_UNICODETOTEXCMMITTF_CID);
NS_UCONV_REG_UNREG(nsUnicodeToTeXCMSYttf, "Unicode", "x-ttf-cmsy",  NS_UNICODETOTEXCMSYTTF_CID);
NS_UCONV_REG_UNREG(nsUnicodeToTeXCMEXttf, "Unicode", "x-ttf-cmex",  NS_UNICODETOTEXCMEXTTF_CID);
#else
NS_UCONV_REG_UNREG(nsUnicodeToTeXCMRt1,  "Unicode", "x-t1-cmr",   NS_UNICODETOTEXCMRT1_CID);
NS_UCONV_REG_UNREG(nsUnicodeToTeXCMMIt1, "Unicode", "x-t1-cmmi",  NS_UNICODETOTEXCMMIT1_CID);
NS_UCONV_REG_UNREG(nsUnicodeToTeXCMSYt1, "Unicode", "x-t1-cmsy",  NS_UNICODETOTEXCMSYT1_CID);
NS_UCONV_REG_UNREG(nsUnicodeToTeXCMEXt1, "Unicode", "x-t1-cmex",  NS_UNICODETOTEXCMEXT1_CID);
#endif
NS_UCONV_REG_UNREG(nsUnicodeToMathematica1, "Unicode", "x-mathematica1",  NS_UNICODETOMATHEMATICA1_CID);
NS_UCONV_REG_UNREG(nsUnicodeToMathematica2, "Unicode", "x-mathematica2",  NS_UNICODETOMATHEMATICA2_CID);
NS_UCONV_REG_UNREG(nsUnicodeToMathematica3, "Unicode", "x-mathematica3",  NS_UNICODETOMATHEMATICA3_CID);
NS_UCONV_REG_UNREG(nsUnicodeToMathematica4, "Unicode", "x-mathematica4",  NS_UNICODETOMATHEMATICA4_CID);
NS_UCONV_REG_UNREG(nsUnicodeToMathematica5, "Unicode", "x-mathematica5",  NS_UNICODETOMATHEMATICA5_CID);
NS_UCONV_REG_UNREG(nsUnicodeToMTExtra, "Unicode", "x-mtextra",  NS_UNICODETOMTEXTRA_CID);

#if defined(XP_PC) || defined(XP_MAC)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToTeXCMRttf);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToTeXCMMIttf);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToTeXCMSYttf);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToTeXCMEXttf);
#else
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToTeXCMRt1);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToTeXCMMIt1);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToTeXCMSYt1);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToTeXCMEXt1);
#endif
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToMathematica1);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToMathematica2);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToMathematica3);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToMathematica4);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToMathematica5);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToMTExtra);

static nsModuleComponentInfo components[] = 
{
#if defined(XP_PC) || defined(XP_MAC)
  { 
    ENCODER_NAME_BASE "x-ttf-cmr" , NS_UNICODETOTEXCMRTTF_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-ttf-cmr",
    nsUnicodeToTeXCMRttfConstructor, 
    nsUnicodeToTeXCMRttfRegSelf, nsUnicodeToTeXCMRttfUnRegSelf
  },
  { 
    ENCODER_NAME_BASE "x-ttf-cmmi" , NS_UNICODETOTEXCMMITTF_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-ttf-cmmi",
    nsUnicodeToTeXCMMIttfConstructor, 
    nsUnicodeToTeXCMMIttfRegSelf, nsUnicodeToTeXCMMIttfUnRegSelf
  },
  { 
    ENCODER_NAME_BASE "x-ttf-cmsy" , NS_UNICODETOTEXCMSYTTF_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-ttf-cmsy",
    nsUnicodeToTeXCMSYttfConstructor, 
    nsUnicodeToTeXCMSYttfRegSelf, nsUnicodeToTeXCMSYttfUnRegSelf
  },
  { 
    ENCODER_NAME_BASE "x-ttf-cmex" , NS_UNICODETOTEXCMEXTTF_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-ttf-cmex",
    nsUnicodeToTeXCMEXttfConstructor, 
    nsUnicodeToTeXCMEXttfRegSelf, nsUnicodeToTeXCMEXttfUnRegSelf
  },
#else
  { 
    ENCODER_NAME_BASE "x-t1-cmr" , NS_UNICODETOTEXCMRT1_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-t1-cmr",
    nsUnicodeToTeXCMRt1Constructor, 
    nsUnicodeToTeXCMRt1RegSelf, nsUnicodeToTeXCMRt1UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "x-t1-cmmi" , NS_UNICODETOTEXCMMIT1_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-t1-cmmi",
    nsUnicodeToTeXCMMIt1Constructor, 
    nsUnicodeToTeXCMMIt1RegSelf, nsUnicodeToTeXCMMIt1UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "x-t1-cmsy" , NS_UNICODETOTEXCMSYT1_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-t1-cmsy",
    nsUnicodeToTeXCMSYt1Constructor, 
    nsUnicodeToTeXCMSYt1RegSelf, nsUnicodeToTeXCMSYt1UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "x-t1-cmex" , NS_UNICODETOTEXCMEXT1_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-t1-cmex",
    nsUnicodeToTeXCMEXt1Constructor, 
    nsUnicodeToTeXCMEXt1RegSelf, nsUnicodeToTeXCMEXt1UnRegSelf
  },
#endif
  { 
    ENCODER_NAME_BASE "x-mathematica1" , NS_UNICODETOMATHEMATICA1_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mathematica1",
    nsUnicodeToMathematica1Constructor, 
    nsUnicodeToMathematica1RegSelf, nsUnicodeToMathematica1UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "x-mathematica2" , NS_UNICODETOMATHEMATICA2_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mathematica2",
    nsUnicodeToMathematica2Constructor, 
    nsUnicodeToMathematica2RegSelf, nsUnicodeToMathematica2UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "x-mathematica3" , NS_UNICODETOMATHEMATICA3_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mathematica3",
    nsUnicodeToMathematica3Constructor, 
    nsUnicodeToMathematica3RegSelf, nsUnicodeToMathematica3UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "x-mathematica4" , NS_UNICODETOMATHEMATICA4_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mathematica4",
    nsUnicodeToMathematica4Constructor, 
    nsUnicodeToMathematica4RegSelf, nsUnicodeToMathematica4UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "x-mathematica5" , NS_UNICODETOMATHEMATICA5_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mathematica5",
    nsUnicodeToMathematica5Constructor, 
    nsUnicodeToMathematica5RegSelf, nsUnicodeToMathematica5UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "x-mtextra" , NS_UNICODETOMTEXTRA_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mtextra",
    nsUnicodeToMTExtraConstructor, 
    nsUnicodeToMTExtraRegSelf, nsUnicodeToMTExtraUnRegSelf
  }
};

NS_IMPL_NSGETMODULE(nsUCvMathModule, components);

