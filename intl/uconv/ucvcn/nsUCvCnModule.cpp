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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#define NS_IMPL_IDS

#include "nspr.h"
#include "nsString.h"
#include "pratom.h"
#include "nsCOMPtr.h"
#include "nsIFactory.h"
#include "nsIRegistry.h"
#include "nsIGenericFactory.h"
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
#include "nsIModule.h"
#include "nsUCvCnCID.h"
#include "nsUCvCnDll.h"

#include "nsHZToUnicode.h"
#include "nsUnicodeToHZ.h"
#include "nsGBKToUnicode.h"
#include "nsUnicodeToGBK.h"
#include "nsUnicodeToGBKNoAscii.h"
#include "nsCP936ToUnicode.h"
#include "nsUnicodeToCP936.h"
#include "nsGB2312ToUnicodeV2.h"
#include "nsUnicodeToGB2312V2.h"
#include "nsUnicodeToGB2312GL.h"
#include "gbku.h"

//----------------------------------------------------------------------------
// Global functions and data [declaration]

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

#define DECODER_NAME_BASE "Unicode Decoder-"
#define ENCODER_NAME_BASE "Unicode Encoder-"

//----------------------------------------------------------------------------


NS_IMPL_NSUCONVERTERREGSELF

NS_UCONV_REG_UNREG(nsGB2312ToUnicodeV2, "GB2312", "Unicode" , NS_GB2312TOUNICODE_CID);
NS_UCONV_REG_UNREG(nsUnicodeToGB2312V2, "Unicode", "GB2312",  NS_UNICODETOGB2312_CID);
NS_UCONV_REG_UNREG(nsCP936ToUnicode, "windows-936", "Unicode" , NS_CP936TOUNICODE_CID);
NS_UCONV_REG_UNREG(nsUnicodeToCP936, "Unicode", "windows-936",  NS_UNICODETOCP936_CID);
NS_UCONV_REG_UNREG(nsGBKToUnicode, "x-gbk", "Unicode" , NS_GBKTOUNICODE_CID);
NS_UCONV_REG_UNREG(nsUnicodeToGBK, "Unicode", "x-gbk",  NS_UNICODETOGBK_CID);
NS_UCONV_REG_UNREG(nsUnicodeToGBKNoAscii, "Unicode", "x-gbk-noascii",  NS_UNICODETOGBKNOASCII_CID);
NS_UCONV_REG_UNREG(nsHZToUnicode, "HZ-GB-2312", "Unicode" , NS_HZTOUNICODE_CID);
NS_UCONV_REG_UNREG(nsUnicodeToHZ, "Unicode", "HZ-GB-2312",  NS_UNICODETOHZ_CID);
NS_UCONV_REG_UNREG(nsUnicodeToGB2312GL, "Unicode", "gb_2312-80",  NS_UNICODETOGB2312GL_CID);
NS_UCONV_REG_UNREG(nsGB18030ToUnicode, "gb18030", "Unicode" , NS_GB18030TOUNICODE_CID);
NS_UCONV_REG_UNREG(nsUnicodeToGB18030, "Unicode", "gb18030",  NS_UNICODETOGB18030_CID);
NS_UCONV_REG_UNREG(nsUnicodeToGB18030Font0, "Unicode", "gb18030.2000-0",  NS_UNICODETOGB18030Font0_CID);
NS_UCONV_REG_UNREG(nsUnicodeToGB18030Font1, "Unicode", "gb18030.2000-1",  NS_UNICODETOGB18030Font1_CID);

NS_GENERIC_FACTORY_CONSTRUCTOR(nsGB2312ToUnicodeV2);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToGB2312V2);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCP936ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToCP936);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsGBKToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToGBK);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToGBKNoAscii);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHZToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToHZ);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToGB2312GL);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsGB18030ToUnicode);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToGB18030);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToGB18030Font0);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToGB18030Font1);


static nsModuleComponentInfo components[] = 
{
  { 
    DECODER_NAME_BASE "GB2312" , NS_GB2312TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "GB2312",
    nsGB2312ToUnicodeV2Constructor ,
    nsGB2312ToUnicodeV2RegSelf , nsGB2312ToUnicodeV2UnRegSelf 
  },
  { 
    ENCODER_NAME_BASE "GB2312" , NS_UNICODETOGB2312_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "GB2312",
    nsUnicodeToGB2312V2Constructor, 
    nsUnicodeToGB2312V2RegSelf, nsUnicodeToGB2312V2UnRegSelf
  },
  { 
    DECODER_NAME_BASE "windows-936" , NS_CP936TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "windows-936",
    nsCP936ToUnicodeConstructor ,
    nsCP936ToUnicodeRegSelf , nsCP936ToUnicodeUnRegSelf 
  },
  { 
    ENCODER_NAME_BASE "windows-936" , NS_UNICODETOCP936_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "windows-936",
    nsUnicodeToCP936Constructor, 
    nsUnicodeToCP936RegSelf, nsUnicodeToCP936UnRegSelf
  },
  { 
    DECODER_NAME_BASE "x-gbk" , NS_GBKTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "x-gbk",
    nsGBKToUnicodeConstructor ,
    nsGBKToUnicodeRegSelf , nsGBKToUnicodeUnRegSelf 
  },
  { 
    ENCODER_NAME_BASE "x-gbk" , NS_UNICODETOGBK_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-gbk",
    nsUnicodeToGBKConstructor, 
    nsUnicodeToGBKRegSelf, nsUnicodeToGBKUnRegSelf
  },  
  { 
    ENCODER_NAME_BASE "x-gbk-noascii" , NS_UNICODETOGBKNOASCII_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "x-gbk-noascii",
    nsUnicodeToGBKNoAsciiConstructor, 
    nsUnicodeToGBKNoAsciiRegSelf, nsUnicodeToGBKNoAsciiUnRegSelf
  },  
  { 
    DECODER_NAME_BASE "HZ-GB-2312" , NS_HZTOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "HZ-GB-2312",
    nsHZToUnicodeConstructor ,
    nsHZToUnicodeRegSelf , nsHZToUnicodeUnRegSelf 
  },  
  { 
    ENCODER_NAME_BASE "HZ-GB-2312" , NS_UNICODETOHZ_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "HZ-GB-2312",
    nsUnicodeToHZConstructor, 
    nsUnicodeToHZRegSelf, nsUnicodeToHZUnRegSelf
  },  
  { 
    ENCODER_NAME_BASE "gb_2312-80" , NS_UNICODETOGB2312GL_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "gb_2312-80",
    nsUnicodeToGB2312GLConstructor, 
    nsUnicodeToGB2312GLRegSelf, nsUnicodeToGB2312GLUnRegSelf
  },
  { 
    DECODER_NAME_BASE "gb18030" , NS_GB18030TOUNICODE_CID, 
    NS_UNICODEDECODER_CONTRACTID_BASE "gb18030",
    nsGB18030ToUnicodeConstructor ,
    nsGB18030ToUnicodeRegSelf , nsGB18030ToUnicodeUnRegSelf 
  },  
  { 
    ENCODER_NAME_BASE "gb18030.2000-0" , NS_UNICODETOGB18030Font0_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "gb18030.2000-0",
    nsUnicodeToGB18030Font0Constructor, 
    nsUnicodeToGB18030Font0RegSelf, nsUnicodeToGB18030Font0UnRegSelf
  },  
  { 
    ENCODER_NAME_BASE "gb18030.2000-1" , NS_UNICODETOGB18030Font1_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "gb18030.2000-1",
    nsUnicodeToGB18030Font1Constructor, 
    nsUnicodeToGB18030Font1RegSelf, nsUnicodeToGB18030Font1UnRegSelf
  },  
  { 
    ENCODER_NAME_BASE "gb18030" , NS_UNICODETOGB18030_CID, 
    NS_UNICODEENCODER_CONTRACTID_BASE "gb18030",
    nsUnicodeToGB18030Constructor, 
    nsUnicodeToGB18030RegSelf, nsUnicodeToGB18030UnRegSelf
  }
};

NS_IMPL_NSGETMODULE(nsUCvCnModule, components);
