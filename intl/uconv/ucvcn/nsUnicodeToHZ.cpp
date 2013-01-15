/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
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
  mHZState = HZ_STATE_ASCII;	// per HZ spec, default to HZ mode
}
NS_IMETHODIMP nsUnicodeToHZ::ConvertNoBuff(
  const PRUnichar * aSrc, 
  int32_t * aSrcLength, 
  char * aDest, 
  int32_t * aDestLength)
{
  int32_t i=0;
  int32_t iSrcLength = *aSrcLength;
  int32_t iDestLength = 0;

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
      if(mUtil.UnicodeToGBKChar(*aSrc, true, &aDest[0], &aDest[1])) {
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

NS_IMETHODIMP nsUnicodeToHZ::FinishNoBuff(char * aDest, int32_t * aDestLength)
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
