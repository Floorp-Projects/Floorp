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
 * The Initial Developer of the Original Code is Intel
 * Corporation. Portions created by Intel are
 * Copyright (C) 1999 Intel Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): Yueheng Xu <yuheng.xu@intel.com>
 *
 * This file is a newer implementation of GB2312 converters developed by
 * Intel Corporation intended to replace the existing mozilla GB2312 converters
 * developed by Netscape Communications in mozilla. 
 *
 */
// =======================================================================
// Original Author: Yueheng Xu
// email: yueheng.xu@intel.com
// phone: (503)264-2248
// Intel Corporation, Oregon, USA
// Last Update: September 7, 1999
// Revision History: 
// 09/07/1999 - initial version. Based on exisiting nsGB2312ToUnicode.cpp. Removed
//              dependecy on those shift/map tables. Now we only depend on gbku.h.
// 09/28/1999 - changed to use the same table and code as GBKToUnicode converter
// ======================================================================================

#if 0
#include "nsGB2312ToUnicodeV2.h"
#include "nsUCvCnDll.h"
#include "gbku.h"
//----------------------------------------------------------------------
// Class nsGB2312ToUnicodeV2 [implementation]
//----------------------------------------------------------------------
// Subclassing of nsTablesDecoderSupport class [implementation]
NS_IMETHODIMP nsGB2312ToUnicodeV2::GetMaxLength(const char * aSrc, 
                                              PRInt32 aSrcLength, 
                                              PRInt32 * aDestLength)
{
  *aDestLength = aSrcLength;
  return NS_OK;
}
NS_IMETHODIMP nsGB2312ToUnicodeV2::ConvertNoBuff(const char* aSrc,
                                                 PRInt32 * aSrcLength,
                                                 PRUnichar *aDest,
                                                 PRInt32 * aDestLength)
{
  PRInt32 i=0;
  PRInt32 iSrcLength = (*aSrcLength);
  PRInt32 iDestlen = 0;
  nsresult rv = NS_OK;
  for (i=0;i<iSrcLength;i++)
  {
    if ( iDestlen >= (*aDestLength) )
    {
      rv = NS_OK_UDEC_MOREOUTPUT;
      break;
    }
    if(UINT8_IN_RANGE(0xa1, *aSrc, 0xfe))
    {
      if(i+1 >= iSrcLength)
      {
        rv = NS_OK_UDEC_MOREINPUT;
        break;
      }
      // To make sure, the second byte has to be checked as well
      // The valid 2nd byte range: [0xA1,0xFE]
      if(UINT8_IN_RANGE(0xa1, aSrc[1], 0xfe))
      {
        // Valid GB 2312 code point
        *aDest = mUtil.GBKCharToUnicode(aSrc[0], aSrc[1]);
        aSrc += 2;
        i++;
      } else {
        // Invalid GB 2312 code point 
        *aDest =  UCS2_NO_MAPPING;
        aSrc++;
      }
    } else {
      if(IS_ASCII(*aSrc))
      {
        // The source is an ASCII
        *aDest = CAST_CHAR_TO_UNICHAR(*aSrc);
      } else {
        *aDest =  UCS2_NO_MAPPING;
      }
      aSrc++;
    }
    iDestlen++;
    aDest++;
    *aSrcLength = i+1;
  }
  *aDestLength = iDestlen;
  return rv;
}
#endif
