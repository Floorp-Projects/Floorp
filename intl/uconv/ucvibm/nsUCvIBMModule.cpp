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
#include "nsUCvIBMCID.h"
#include "nsUCvIBMDll.h"

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

NS_IMPL_NSUCONVERTERREGSELF

NS_UCONV_REG_UNREG(nsCP850ToUnicode, "IBM850", "Unicode" , NS_CP850TOUNICODE_CID);
NS_UCONV_REG_UNREG(nsCP852ToUnicode, "IBM852", "Unicode" , NS_CP852TOUNICODE_CID);
NS_UCONV_REG_UNREG(nsCP855ToUnicode, "IBM855", "Unicode" , NS_CP855TOUNICODE_CID);
NS_UCONV_REG_UNREG(nsCP857ToUnicode, "IBM857", "Unicode" , NS_CP857TOUNICODE_CID);
NS_UCONV_REG_UNREG(nsCP862ToUnicode, "IBM862", "Unicode" , NS_CP862TOUNICODE_CID);
NS_UCONV_REG_UNREG(nsCP864ToUnicode, "IBM864", "Unicode" , NS_CP864TOUNICODE_CID);
NS_UCONV_REG_UNREG(nsCP864iToUnicode,"IBM864i", "Unicode", NS_CP864ITOUNICODE_CID);
NS_UCONV_REG_UNREG(nsUnicodeToCP850, "Unicode", "IBM850",  NS_UNICODETOCP850_CID);
NS_UCONV_REG_UNREG(nsUnicodeToCP852, "Unicode", "IBM852",  NS_UNICODETOCP852_CID);
NS_UCONV_REG_UNREG(nsUnicodeToCP855, "Unicode", "IBM855",  NS_UNICODETOCP855_CID);
NS_UCONV_REG_UNREG(nsUnicodeToCP857, "Unicode", "IBM857",  NS_UNICODETOCP857_CID);
NS_UCONV_REG_UNREG(nsUnicodeToCP862, "Unicode", "IBM862",  NS_UNICODETOCP862_CID);
NS_UCONV_REG_UNREG(nsUnicodeToCP864, "Unicode", "IBM864",  NS_UNICODETOCP864_CID);
NS_UCONV_REG_UNREG(nsUnicodeToCP864i,"Unicode", "IBM864i", NS_UNICODETOCP864I_CID);

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

static nsModuleComponentInfo components[] = 
{
  { 
    DECODER_NAME_BASE "IBM850" , NS_CP850TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "IBM850",
    nsCP850ToUnicodeConstructor ,
    nsCP850ToUnicodeRegSelf , nsCP850ToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "IBM852" , NS_CP852TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "IBM852",
    nsCP852ToUnicodeConstructor ,
    nsCP852ToUnicodeRegSelf , nsCP852ToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "IBM855" , NS_CP855TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "IBM855",
    nsCP855ToUnicodeConstructor ,
    nsCP855ToUnicodeRegSelf , nsCP855ToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "IBM857" , NS_CP857TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "IBM857",
    nsCP857ToUnicodeConstructor ,
    nsCP857ToUnicodeRegSelf , nsCP857ToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "IBM862" , NS_CP862TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "IBM862",
    nsCP862ToUnicodeConstructor ,
    nsCP862ToUnicodeRegSelf , nsCP862ToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "IBM864" , NS_CP864TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "IBM864",
    nsCP864ToUnicodeConstructor ,
    nsCP864ToUnicodeRegSelf , nsCP864ToUnicodeUnRegSelf 
  },
  { 
    DECODER_NAME_BASE "IBM864i" , NS_CP864ITOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "IBM864i",
    nsCP864iToUnicodeConstructor ,
    nsCP864iToUnicodeRegSelf , nsCP864iToUnicodeUnRegSelf 
  },
  { 
    ENCODER_NAME_BASE "IBM850" , NS_UNICODETOCP850_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "IBM850",
    nsUnicodeToCP850Constructor, 
    nsUnicodeToCP850RegSelf, nsUnicodeToCP850UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "IBM852" , NS_UNICODETOCP852_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "IBM852",
    nsUnicodeToCP852Constructor, 
    nsUnicodeToCP852RegSelf, nsUnicodeToCP852UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "IBM855" , NS_UNICODETOCP855_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "IBM855",
    nsUnicodeToCP855Constructor, 
    nsUnicodeToCP855RegSelf, nsUnicodeToCP855UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "IBM857" , NS_UNICODETOCP857_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "IBM857",
    nsUnicodeToCP857Constructor, 
    nsUnicodeToCP857RegSelf, nsUnicodeToCP857UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "IBM862" , NS_UNICODETOCP862_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "IBM862",
    nsUnicodeToCP862Constructor, 
    nsUnicodeToCP862RegSelf, nsUnicodeToCP862UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "IBM864" , NS_UNICODETOCP864_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "IBM864",
    nsUnicodeToCP864Constructor, 
    nsUnicodeToCP864RegSelf, nsUnicodeToCP864UnRegSelf
  },
  { 
    ENCODER_NAME_BASE "IBM864i" , NS_UNICODETOCP864I_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "IBM864i",
    nsUnicodeToCP864iConstructor, 
    nsUnicodeToCP864iRegSelf, nsUnicodeToCP864iUnRegSelf
  }
};

NS_IMPL_NSGETMODULE(nsUCvIBMModule, components);

