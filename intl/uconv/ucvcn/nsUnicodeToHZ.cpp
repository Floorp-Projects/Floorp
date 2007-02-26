/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 /**
 * A character set converter from Unicode to HZ.
 * 
 *
 * @created         08/Sept/1999
 * @author  Yueheng Xu, Yueheng.Xu@intel.com
 * Revision History
 * 04/Oct/1999. Yueheng Xu: Fixed line continuation problem when line 
 *                          ended by '~';
 *              Used table UnicodeToGBK[] to speed up the mapping.
 */
#include "nsUnicodeToHZ.h"
#include "nsUCvCnDll.h"
#include "gbku.h"
//----------------------------------------------------------------------
// Class nsUnicodeToGBK [implementation]
#define HZ_STATE_GB		1
#define HZ_STATE_ASCII	2
#define HZ_STATE_TILD	3
#define HZLEAD1 '~'
#define HZLEAD2 '{'
#define HZLEAD3 '}'
#define UNICODE_TILD	0x007E
nsUnicodeToHZ::nsUnicodeToHZ() : nsEncoderSupport(6)
{
  mUtil.InitToGBKTable();
  mHZState = HZ_STATE_ASCII;	// per HZ spec, default to HZ mode
}
NS_IMETHODIMP nsUnicodeToHZ::ConvertNoBuff(
  const PRUnichar * aSrc, 
  PRInt32 * aSrcLength, 
  char * aDest, 
  PRInt32 * aDestLength)
{
  PRInt32 i=0;
  PRInt32 iSrcLength = *aSrcLength;
  PRInt32 iDestLength = 0;

  for (i=0;i< iSrcLength;i++)
  {
    if(! IS_ASCII(*aSrc))
    {
      // hi byte has something, it is not ASCII, process as a GB
      if ( mHZState != HZ_STATE_GB )
      {
        // we are adding a '~{' ESC sequence to star a HZ string
        mHZState = HZ_STATE_GB;
        aDest[0] = '~';
        aDest[1] = '{';
        aDest += 2;	// increment 2 bytes
        iDestLength +=2;
      }
      if(mUtil.UnicodeToGBKChar(*aSrc, PR_TRUE, &aDest[0], &aDest[1])) {
        aDest += 2;	// increment 2 bytes
        iDestLength +=2;
      } else {
        // some thing that we cannot convert
        // xxx fix me ftang
        // error handling here
      }
    } else {
      // this is an ASCII

      // if we are in HZ mode, end it by adding a '~}' ESC sequence
      if ( mHZState == HZ_STATE_GB )
      {
        mHZState = HZ_STATE_ASCII;
        aDest[0] = '~';
        aDest[1] = '}';
        aDest += 2; // increment 2 bytes
        iDestLength +=2;
      }
          
      // if this is a regular char '~' , convert it to two '~'
      if ( *aSrc == UNICODE_TILD )
      {
        aDest[0] = '~';
        aDest[1] = '~';
        aDest += 2; // increment 2 bytes
        iDestLength +=2;
      } else {
        // other regular ASCII chars convert by normal ways
	
        // Is this works for both little endian and big endian machines ?
        *aDest = (char) ( (PRUnichar)(*aSrc) );
        aDest++; // increment 1 byte
        iDestLength +=1;
      }
    }
    aSrc++;	 // increment 2 bytes
    if ( iDestLength >= (*aDestLength) )
    {
      break;
    }
  }
  *aDestLength = iDestLength;
  *aSrcLength = i;
  return NS_OK;
}

NS_IMETHODIMP nsUnicodeToHZ::FinishNoBuff(char * aDest, PRInt32 * aDestLength)
{
  if ( mHZState == HZ_STATE_GB )
  {
    // if we are in HZ mode, end it by adding a '~}' ESC sequence
    mHZState = HZ_STATE_ASCII;
    aDest[0] = '~';
    aDest[1] = '}';
    *aDestLength = 2;
  } else {
    *aDestLength = 0;
  }
  return NS_OK;
}

NS_IMETHODIMP nsUnicodeToHZ::FillInfo(PRUint32 *aInfo)
{
  mUtil.FillGB2312Info(aInfo);
  //GB2312 font lib also have single byte ASCII characters, set them here
  for ( PRUint16 u = 0x0000; u <= 0x007F; u++)
    SET_REPRESENTABLE(aInfo, u);
  return NS_OK;
}
