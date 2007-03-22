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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
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
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "pratom.h"
#include "nspr.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIFactory.h"
#include "nsIGenericFactory.h"
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
#include "nsICategoryManager.h"
#include "nsEncoderDecoderUtils.h"
#include "nsIModule.h"
#include "nsUCvMathCID.h"
#include "nsUCvMathDll.h"
#include "nsCRT.h"

#include "nsUnicodeToTeXCMRttf.h"
#include "nsUnicodeToTeXCMMIttf.h"
#include "nsUnicodeToTeXCMSYttf.h"
#include "nsUnicodeToTeXCMEXttf.h"
#if !defined(XP_WIN) && !defined(XP_OS2) && !defined(XP_MAC) && !defined(XP_MACOSX)
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

#define DECODER_NAME_BASE "Unicode Decoder-"
#define ENCODER_NAME_BASE "Unicode Encoder-"

NS_CONVERTER_REGISTRY_START

NS_UCONV_REG_UNREG_ENCODER("x-ttf-cmr",   NS_UNICODETOTEXCMRTTF_CID)
NS_UCONV_REG_UNREG_ENCODER("x-ttf-cmmi",  NS_UNICODETOTEXCMMITTF_CID)
NS_UCONV_REG_UNREG_ENCODER("x-ttf-cmsy",  NS_UNICODETOTEXCMSYTTF_CID)
NS_UCONV_REG_UNREG_ENCODER("x-ttf-cmex",  NS_UNICODETOTEXCMEXTTF_CID)
#if !defined(XP_WIN) && !defined(XP_OS2) && !defined(XP_MAC) && !defined(XP_MACOSX)
NS_UCONV_REG_UNREG_ENCODER("x-t1-cmr",   NS_UNICODETOTEXCMRT1_CID)
NS_UCONV_REG_UNREG_ENCODER("x-t1-cmmi",  NS_UNICODETOTEXCMMIT1_CID)
NS_UCONV_REG_UNREG_ENCODER("x-t1-cmsy",  NS_UNICODETOTEXCMSYT1_CID)
NS_UCONV_REG_UNREG_ENCODER("x-t1-cmex",  NS_UNICODETOTEXCMEXT1_CID)
#endif
NS_UCONV_REG_UNREG_ENCODER("x-mathematica1",  NS_UNICODETOMATHEMATICA1_CID)
NS_UCONV_REG_UNREG_ENCODER("x-mathematica2",  NS_UNICODETOMATHEMATICA2_CID)
NS_UCONV_REG_UNREG_ENCODER("x-mathematica3",  NS_UNICODETOMATHEMATICA3_CID)
NS_UCONV_REG_UNREG_ENCODER("x-mathematica4",  NS_UNICODETOMATHEMATICA4_CID)
NS_UCONV_REG_UNREG_ENCODER("x-mathematica5",  NS_UNICODETOMATHEMATICA5_CID)
NS_UCONV_REG_UNREG_ENCODER("x-mtextra",  NS_UNICODETOMTEXTRA_CID)

NS_CONVERTER_REGISTRY_END

NS_IMPL_NSUCONVERTERREGSELF

NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToTeXCMRttf)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToTeXCMMIttf)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToTeXCMSYttf)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToTeXCMEXttf)
#if !defined(XP_WIN) && !defined(XP_OS2) && !defined(XP_MAC) && !defined(XP_MACOSX)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToTeXCMRt1)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToTeXCMMIt1)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToTeXCMSYt1)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToTeXCMEXt1)
#endif
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToMathematica1)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToMathematica2)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToMathematica3)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToMathematica4)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToMathematica5)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToMTExtra)

static const nsModuleComponentInfo components[] = 
{
  { 
    ENCODER_NAME_BASE "x-ttf-cmr" , NS_UNICODETOTEXCMRTTF_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-ttf-cmr",
    nsUnicodeToTeXCMRttfConstructor, 
  },
  { 
    ENCODER_NAME_BASE "x-ttf-cmmi" , NS_UNICODETOTEXCMMITTF_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-ttf-cmmi",
    nsUnicodeToTeXCMMIttfConstructor, 
  },
  { 
    ENCODER_NAME_BASE "x-ttf-cmsy" , NS_UNICODETOTEXCMSYTTF_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-ttf-cmsy",
    nsUnicodeToTeXCMSYttfConstructor, 
  },
  { 
    ENCODER_NAME_BASE "x-ttf-cmex" , NS_UNICODETOTEXCMEXTTF_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-ttf-cmex",
    nsUnicodeToTeXCMEXttfConstructor, 
  },
#if !defined(XP_WIN) && !defined(XP_OS2) && !defined(XP_MAC) && !defined(XP_MACOSX)
  { 
    ENCODER_NAME_BASE "x-t1-cmr" , NS_UNICODETOTEXCMRT1_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-t1-cmr",
    nsUnicodeToTeXCMRt1Constructor, 
  },
  { 
    ENCODER_NAME_BASE "x-t1-cmmi" , NS_UNICODETOTEXCMMIT1_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-t1-cmmi",
    nsUnicodeToTeXCMMIt1Constructor, 
  },
  { 
    ENCODER_NAME_BASE "x-t1-cmsy" , NS_UNICODETOTEXCMSYT1_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-t1-cmsy",
    nsUnicodeToTeXCMSYt1Constructor, 
  },
  { 
    ENCODER_NAME_BASE "x-t1-cmex" , NS_UNICODETOTEXCMEXT1_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-t1-cmex",
    nsUnicodeToTeXCMEXt1Constructor, 
  },
#endif
  { 
    ENCODER_NAME_BASE "x-mathematica1" , NS_UNICODETOMATHEMATICA1_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mathematica1",
    nsUnicodeToMathematica1Constructor, 
    nsUConverterRegSelf, nsUConverterUnregSelf,
  },
  { 
    ENCODER_NAME_BASE "x-mathematica2" , NS_UNICODETOMATHEMATICA2_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mathematica2",
    nsUnicodeToMathematica2Constructor, 
  },
  { 
    ENCODER_NAME_BASE "x-mathematica3" , NS_UNICODETOMATHEMATICA3_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mathematica3",
    nsUnicodeToMathematica3Constructor, 
  },
  { 
    ENCODER_NAME_BASE "x-mathematica4" , NS_UNICODETOMATHEMATICA4_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mathematica4",
    nsUnicodeToMathematica4Constructor, 
  },
  { 
    ENCODER_NAME_BASE "x-mathematica5" , NS_UNICODETOMATHEMATICA5_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mathematica5",
    nsUnicodeToMathematica5Constructor, 
  },
  { 
    ENCODER_NAME_BASE "x-mtextra" , NS_UNICODETOMTEXTRA_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-mtextra",
    nsUnicodeToMTExtraConstructor, 
  }
};

NS_IMPL_NSGETMODULE(nsUCvMathModule, components)

