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
 * The Original Code is Mozilla Communicator client code.
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

#include "nsGBKConvUtil.h"
#include "gbku.h"
#include "nsCRT.h"
#define MAX_GBK_LENGTH 24066 /* (0xfe-0x80)*(0xfe-0x3f) */
//--------------------------------------------------------------------
// nsGBKConvUtil
//--------------------------------------------------------------------

static bool gInitToGBKTable = false;
static const PRUnichar gGBKToUnicodeTable[MAX_GBK_LENGTH] = {
#include "cp936map.h"
};
static PRUint16 gUnicodeToGBKTable[0xA000-0x4e00];

bool nsGBKConvUtil::UnicodeToGBKChar(
  PRUnichar aChar, bool aToGL, char* 
  aOutByte1, char* aOutByte2)
{
  NS_ASSERTION(gInitToGBKTable, "gGBKToUnicodeTable is not init yet. need to call InitToGBKTable first");
  bool found=false;
  *aOutByte1 = *aOutByte2 = 0;
  if(UNICHAR_IN_RANGE(0xd800, aChar, 0xdfff))
  {
    // surrogate is not in here
    return PR_FALSE;
  }
  if(UNICHAR_IN_RANGE(0x4e00, aChar, 0x9FFF))
  {
    PRUint16 item = gUnicodeToGBKTable[aChar - 0x4e00];
    if(item != 0) 
    {
      *aOutByte1 = item >> 8;
      *aOutByte2 = item & 0x00FF;
      found = PR_TRUE;
    } else {
      return PR_FALSE;
    }
  } else {
    // ugly linear search
    for( PRInt32 i = 0; i < MAX_GBK_LENGTH; i++ )
    {
      if( aChar == gGBKToUnicodeTable[i])
      {
        *aOutByte1 = (i /  0x00BF + 0x0081) ;
        *aOutByte2 = (i %  0x00BF + 0x0040) ;
        found = PR_TRUE;
        break;
      }
    }
  }
  if(! found)
    return PR_FALSE;

  if(aToGL) {
    // to GL, we only return if it is in the range 
    if(UINT8_IN_RANGE(0xA1, *aOutByte1, 0xFE) &&
       UINT8_IN_RANGE(0xA1, *aOutByte2, 0xFE))
    {
      // mask them to GL 
      *aOutByte1 &= 0x7F;
      *aOutByte2 &= 0x7F;
    } else {
      // if it does not fit into 0xa1-0xfe 0xa1-0xfe range that mean
      // it is not a GB2312 character, we cannot map to GL 
      *aOutByte1 = 0x00;
      *aOutByte2 = 0x00;
      return PR_FALSE;
    }
  }
  return PR_TRUE;
}
PRUnichar nsGBKConvUtil::GBKCharToUnicode(char aByte1, char aByte2)
{
  NS_ASSERTION(UINT8_IN_RANGE(0x81,aByte1, 0xFE), "first byte out of range");
  NS_ASSERTION(UINT8_IN_RANGE(0x40,aByte2, 0xFE), "second byte out of range");

  PRUint8 i1 = (PRUint8)aByte1;
  PRUint8 i2 = (PRUint8)aByte2;
  PRUint16 idx = (i1 - 0x0081) * 0x00bf + i2 - 0x0040 ;

  NS_ASSERTION(idx < MAX_GBK_LENGTH, "ARB");
  // play it safe- add if statement here ot protect ARB
  // probably not necessary
  if(idx < MAX_GBK_LENGTH)
    return gGBKToUnicodeTable[ idx ];
  else
    return UCS2_NO_MAPPING;
}
void nsGBKConvUtil::InitToGBKTable()
{
  if ( gInitToGBKTable )
   return;

  PRUnichar unicode;
  PRUnichar i;
  // zap it to zero first
  memset(gUnicodeToGBKTable,0, sizeof(gUnicodeToGBKTable));

  for ( i=0; i<MAX_GBK_LENGTH; i++ )
  {
    unicode = gGBKToUnicodeTable[i];
    // to reduce size of gUnicodeToGBKTable, we only do direct unicode to GB 
    // table mapping between unicode 0x4E00 and 0xA000. Others by searching
    // gGBKToUnicodeTable. There is a trade off between memory usage and speed.
    if(UNICHAR_IN_RANGE(0x4e00, unicode, 0x9fff))
    {
      unicode -= 0x4E00; 
      gUnicodeToGBKTable[unicode] =  (( i / 0x00BF + 0x0081) << 8) | 
                                    ( i % 0x00BF+ 0x0040);
    }
  }
  gInitToGBKTable = PR_TRUE;
}
