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
 */

#include "nsGBKConvUtil.h"
#include "gbku.h"
#include "nsCRT.h"
#include "nsICharRepresentable.h"
#define MAX_GBK_LENGTH 24066 /* (0xfe-0x80)*(0xfe-0x3f) */
//--------------------------------------------------------------------
// nsGBKConvUtil
//--------------------------------------------------------------------

static PRBool gInitToGBKTable = PR_FALSE;
static PRUnichar gGBKToUnicodeTable[MAX_GBK_LENGTH] = {
#include "cp936map.h"
};
static PRUint16 gUnicodeToGBKTable[0xA000-0x4e00];

PRBool nsGBKConvUtil::UnicodeToGBKChar(
  PRUnichar aChar, PRBool aToGL, char* 
  aOutByte1, char* aOutByte2)
{
  NS_ASSERTION(gInitToGBKTable, "gGBKToUnicodeTable is not init yet. need to call InitToGBKTable first");
  PRBool found=PR_FALSE;
  *aOutByte1 = *aOutByte2 = 0;
  if(UNICHAR_IN_RANGE(0x4e00, aChar, 0x9FFF))
  {
    PRUint16 item = gUnicodeToGBKTable[aChar - 0x4e00];
    *aOutByte1 = item >> 8;
    *aOutByte2 = item & 0x00FF;
    found = PR_TRUE;
  }
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
  nsCRT::memset(gUnicodeToGBKTable,0, sizeof(gUnicodeToGBKTable));

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
void nsGBKConvUtil::FillInfo( 
  PRUint32 *aInfo, 
  PRUint8 aStart1, PRUint8 aEnd1, 
  PRUint8 aStart2, PRUint8 aEnd2
)
{
  PRUint16 i,j, k;
  PRUnichar unicode;

  for ( i=aStart1; i<=aEnd1; i++) 
  {
    for( j=aStart2; j<=aEnd2; j++)
    {
      k = (i - 0x0081)*0x00BF +(j-0x0040);    
      unicode = gGBKToUnicodeTable[k];
      NS_ASSERTION(unicode != 0xFFFF, "somehow the table still use 0xffff");
      if (unicode != UCS2_NO_MAPPING) 
      {
        SET_REPRESENTABLE(aInfo, unicode);
      }               
    }
  }                   
}
void nsGBKConvUtil::FillGB2312Info( 
  PRUint32 *aInfo
)
{
  // The following range is coded by looking at the GB2312 standard
  // and make sure we do not call FillInfo for undefined code point
  // Symbol
  // row 1 - 1 range (full)
  FillInfo(aInfo, 0x21|0x80, 0x21|0x80, 0x21|0x80, 0x7E|0x80);
  // row 2 - 3 range
  FillInfo(aInfo, 0x22|0x80, 0x22|0x80, (0x20+17)|0x80, (0x20+66)|0x80);
  FillInfo(aInfo, 0x22|0x80, 0x22|0x80, (0x20+69)|0x80, (0x20+78)|0x80);
  FillInfo(aInfo, 0x22|0x80, 0x22|0x80, (0x20+81)|0x80, (0x20+92)|0x80);
  // row 3 - 1 range (full)
  FillInfo(aInfo, 0x23|0x80, 0x23|0x80, 0x21|0x80, 0x7E|0x80);
  // row 4 - 1 range
  FillInfo(aInfo, 0x24|0x80, 0x24|0x80, (0x20+ 1)|0x80, (0x20+83)|0x80);
  // row 5 - 1 range
  FillInfo(aInfo, 0x25|0x80, 0x25|0x80, (0x20+ 1)|0x80, (0x20+86)|0x80);
  // row 6 - 2 range
  FillInfo(aInfo, 0x26|0x80, 0x26|0x80, (0x20+ 1)|0x80, (0x20+24)|0x80);
  FillInfo(aInfo, 0x26|0x80, 0x26|0x80, (0x20+33)|0x80, (0x20+56)|0x80);
  // row 7
  FillInfo(aInfo, 0x27|0x80, 0x27|0x80, (0x20+ 1)|0x80, (0x20+33)|0x80);
  FillInfo(aInfo, 0x27|0x80, 0x27|0x80, (0x20+49)|0x80, (0x20+81)|0x80);
  // row 8
  FillInfo(aInfo, 0x28|0x80, 0x28|0x80, (0x20+ 1)|0x80, (0x20+26)|0x80);
  FillInfo(aInfo, 0x28|0x80, 0x28|0x80, (0x20+36)|0x80, (0x20+73)|0x80);
  // row 9
  FillInfo(aInfo, 0x29|0x80, 0x29|0x80, (0x20+ 4)|0x80, (0x20+79)|0x80);
 
  // Frequent used Hanzi
  // 3021-567e
  FillInfo(aInfo, 0x30|0x80, 0x56|0x80, 0x21|0x80, 0x7E|0x80);
  // 5721-5779
  FillInfo(aInfo, 0x57|0x80, 0x57|0x80, 0x21|0x80, 0x79|0x80);

  // Infrequent used Hanzi
  // 5821-777e
  FillInfo(aInfo, 0x58|0x80, 0x77|0x80, 0x21|0x80, 0x7E|0x80);
}
