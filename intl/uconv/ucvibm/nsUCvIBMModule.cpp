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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   IBM Corporation
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 1999
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 12/09/1999   IBM Corp.       Support for IBM codepages - 850,852,855,857,862,864
 *
 */

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
#include "nsUCvIBMCID.h"
#include "nsUCvIBMDll.h"
#include "nsCRT.h"

#include "nsCP850ToUnicode.h"
#include "nsCP852ToUnicode.h"
#include "nsCP855ToUnicode.h"
#include "nsCP857ToUnicode.h"
#include "nsCP862ToUnicode.h"
#include "nsCP864ToUnicode.h"
#include "nsCP864iToUnicode.h"
#include "nsUnicodeToCP850.h"
#include "nsUnicodeToCP852.h"
#include "nsUnicodeToCP855.h"
#include "nsUnicodeToCP857.h"
#include "nsUnicodeToCP862.h"
#include "nsUnicodeToCP864.h"
#include "nsUnicodeToCP864i.h"
//----------------------------------------------------------------------------
// Global functions and data [declaration]

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

#define DECODER_NAME_BASE "Unicode Decoder-"
#define ENCODER_NAME_BASE "Unicode Encoder-"

NS_CONVERTER_REGISTRY_START

NS_UCONV_REG_UNREG("IBM850", "Unicode" , NS_CP850TOUNICODE_CID)
NS_UCONV_REG_UNREG("IBM852", "Unicode" , NS_CP852TOUNICODE_CID)
NS_UCONV_REG_UNREG("IBM855", "Unicode" , NS_CP855TOUNICODE_CID)
NS_UCONV_REG_UNREG("IBM857", "Unicode" , NS_CP857TOUNICODE_CID)
NS_UCONV_REG_UNREG("IBM862", "Unicode" , NS_CP862TOUNICODE_CID)
NS_UCONV_REG_UNREG("IBM864", "Unicode" , NS_CP864TOUNICODE_CID)
NS_UCONV_REG_UNREG("IBM864i", "Unicode", NS_CP864ITOUNICODE_CID)
NS_UCONV_REG_UNREG("Unicode", "IBM850",  NS_UNICODETOCP850_CID)
NS_UCONV_REG_UNREG("Unicode", "IBM852",  NS_UNICODETOCP852_CID)
NS_UCONV_REG_UNREG("Unicode", "IBM855",  NS_UNICODETOCP855_CID)
NS_UCONV_REG_UNREG("Unicode", "IBM857",  NS_UNICODETOCP857_CID)
NS_UCONV_REG_UNREG("Unicode", "IBM862",  NS_UNICODETOCP862_CID)
NS_UCONV_REG_UNREG("Unicode", "IBM864",  NS_UNICODETOCP864_CID)
NS_UCONV_REG_UNREG("Unicode", "IBM864i", NS_UNICODETOCP864I_CID)

NS_CONVERTER_REGISTRY_END

NS_IMPL_NSUCONVERTERREGSELF

NS_GENERIC_FACTORY_CONSTRUCTOR(nsCP850ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCP852ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCP855ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCP857ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCP862ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCP864ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCP864iToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToCP850);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToCP852);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToCP855);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToCP857);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToCP862);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToCP864);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToCP864i);

static const nsModuleComponentInfo components[] = 
{
  { 
    DECODER_NAME_BASE "IBM850" , NS_CP850TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "IBM850",
    nsCP850ToUnicodeConstructor ,
    // global converter registration
    nsUConverterRegSelf,
    nsUConverterUnregSelf,
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
  }
};

NS_IMPL_NSGETMODULE(nsUCvIBMModule, components);

