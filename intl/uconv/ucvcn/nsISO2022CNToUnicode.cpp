/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Contributor(s): Ervin Yan <ervin.yan@sun.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsISO2022CNToUnicode.h"
#include "nsUCSupport.h"
#include "nsICharsetConverterManager.h"
#include "nsIServiceManager.h"

static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

NS_IMETHODIMP nsISO2022CNToUnicode::GB2312_To_Unicode(unsigned char *aSrc, PRInt32 aSrcLength, PRUnichar * aDest, PRInt32 * aDestLength)
{
    nsresult rv;

    if(!mGB2312_Decoder) {
       // creating a delegate converter (GB2312)
       nsCOMPtr<nsICharsetConverterManager> ccm =
              do_GetService(kCharsetConverterManagerCID, &rv);
       if(NS_FAILED(rv))
          return NS_ERROR_UNEXPECTED;

       rv = ccm->GetUnicodeDecoderRaw("GB2312", getter_AddRefs(mGB2312_Decoder));
       if(NS_FAILED(rv))
          return NS_ERROR_UNEXPECTED;
    }

    if(!mGB2312_Decoder) // failed creating a delegate converter
       return NS_ERROR_UNEXPECTED;

    rv = mGB2312_Decoder->Convert((const char *)aSrc, &aSrcLength, aDest, aDestLength);
    return rv;
}

NS_IMETHODIMP nsISO2022CNToUnicode::EUCTW_To_Unicode(unsigned char *aSrc, PRInt32 aSrcLength, PRUnichar * aDest, PRInt32 * aDestLength)
{
    nsresult rv;

    if(!mEUCTW_Decoder) {
       // creating a delegate converter (x-euc-tw)
       nsCOMPtr<nsICharsetConverterManager> ccm =
              do_GetService(kCharsetConverterManagerCID, &rv);
       if(NS_FAILED(rv))
          return NS_ERROR_UNEXPECTED;

       rv = ccm->GetUnicodeDecoderRaw("x-euc-tw", getter_AddRefs(mEUCTW_Decoder));
       if(NS_FAILED(rv))
          return NS_ERROR_UNEXPECTED;
    }

    if(!mEUCTW_Decoder) // failed creating a delegate converter
       return NS_ERROR_UNEXPECTED;

    rv = mEUCTW_Decoder->Convert((const char *)aSrc, &aSrcLength, aDest, aDestLength);
    return(rv);
}

NS_IMETHODIMP nsISO2022CNToUnicode::Convert(const char * aSrc, PRInt32 * aSrcLen, PRUnichar * aDest, PRInt32 * aDestLen)
{
  const unsigned char * srcEnd = (unsigned char *)aSrc + *aSrcLen;
  const unsigned char * src = (unsigned char *) aSrc;
  PRUnichar* destEnd = aDest + *aDestLen;
  PRUnichar* dest = aDest;
  nsresult rv;
  PRInt32 aLen; 

  while ((src < srcEnd))
  {
    switch (mState)
    {
      case eState_ASCII:
        if(ESC == *src) {
           mState = eState_ESC;
        } else {
           if(dest+1 >= destEnd)
              goto error1;
           *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;

           mState = eState_ASCII;
        }
        break;

      case eState_ESC:    // ESC
        if('$' == *src) {
           mState = eState_ESC_24;
        } else {
           if(dest+2 >= destEnd)
              goto error1;
           *dest++ = (PRUnichar) ESC;
           *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;

           mState = eState_ASCII;
        }
        break;

      case eState_ESC_24: // ESC $
        if(')' == *src) {
           mState = eState_ESC_24_29;
        } else if('*' == *src) {
           mState = eState_ESC_24_2A;
        } else if('+' == *src) {
           mState = eState_ESC_24_2B;
        } else {
           if(dest+3 >= destEnd)
              goto error1;
           *dest++ = (PRUnichar) ESC;
           *dest++ = (PRUnichar) '$';
           *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;

           mState = eState_ASCII;
        }
        break;

      case eState_ESC_24_29: // ESC $ )
        if('A' == *src) {
           mState = eState_ESC_24_29_A;
        } else if('G' == *src) {
           mState = eState_ESC_24_29_G;
        } else {
           if(dest+4 >= destEnd)
              goto error1;
           *dest++ = (PRUnichar) ESC;
           *dest++ = (PRUnichar) '$';
           *dest++ = (PRUnichar) ')';
           *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;

           mState = eState_ASCII;
        }
        break;

      case eState_ESC_24_29_A:  // ESC $ ) A
        if(SO == *src) {
           mState = eState_GB2312_1980;
        } else {
           if(dest+5 >= destEnd)
              goto error1;
           *dest++ = (PRUnichar) ESC;
           *dest++ = (PRUnichar) '$';
           *dest++ = (PRUnichar) ')';
           *dest++ = (PRUnichar) 'A';
           *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;

           mState = eState_ASCII;
        }
        break;

      case eState_GB2312_1980:   // ESC $ ) A SO
        if(SI == *src) { // Shift-In (SI)
           mState = eState_ESC_24_29_A_SO_SI;
        } else if(ESC == *src) {
           mState = eState_ESC;
        } else {
           if(0x20 < *src && *src < 0x7f) {
              mData = *src;
              mState = eState_GB2312_1980_2ndbyte;
           } else {
              if(dest+1 >= destEnd)
                 goto error1;
              *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;
           }
        }
        break; 

      case eState_GB2312_1980_2ndbyte:  // ESC $ ) A SO
        if(0x20 < *src && *src < 0x7f) {
           unsigned char gb[2];
           PRInt32 gbLen = 2;

           gb[0] = mData | 0x80;
           gb[1] = *src | 0x80;

           aLen = destEnd - dest;
           rv = GB2312_To_Unicode(gb, gbLen, dest, &aLen);
           if(rv == NS_OK_UDEC_MOREOUTPUT) {
              goto error1;
           } else if(NS_FAILED(rv)) {
              goto error2;
           }

           dest += aLen;
        } else {
           if(dest+2 >= destEnd)
              goto error1;
           *dest++ = (PRUnichar) mData;
           *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;
        }
        mState = eState_GB2312_1980;
        break;

      case eState_ESC_24_29_A_SO_SI:  // ESC $ ) A SO SI
        if(SO == *src) {
           mState = eState_GB2312_1980;
        } else if(ESC == *src) {
           mState = eState_ESC;
        } else {
           if(dest+1 >= destEnd)
              goto error1;
           *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;

           mState = eState_ESC_24_29_A_SO_SI;
        }
        break;

      case eState_ESC_24_29_G:   // ESC $ ) G
        if(SO == *src) {
           mState = eState_CNS11643_1;
        } else {
           if(dest+5 >= destEnd)
              goto error1;
           *dest++ = (PRUnichar) ESC;
           *dest++ = (PRUnichar) '$';
           *dest++ = (PRUnichar) ')';
           *dest++ = (PRUnichar) 'G';
           *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;

           mState = eState_ASCII;
        }
        break;

      case eState_CNS11643_1:   // ESC $ ) G SO
        if(SI == *src) { // Shift-In (SI)
           mState = eState_ESC_24_29_G_SO_SI;
        } else if(ESC == *src) {
           mState = eState_ESC;
        } else {
           if(0x20 < *src && *src < 0x7f) {
              mData = *src;
              mState = eState_CNS11643_1_2ndbyte;
           } else {
              if(dest+1 >= destEnd)
                 goto error1;
              *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;
           }
        }
        break;

      case eState_CNS11643_1_2ndbyte:  // ESC $ ) G SO
        if(0x20 < *src && *src < 0x7f) {
           unsigned char cns[4];
           PRInt32 cnsLen = 2;

           cns[0] = mData | 0x80;
           cns[1] = *src | 0x80;

           aLen = destEnd - dest;
           rv = EUCTW_To_Unicode(cns, cnsLen, dest, &aLen);
           if(rv == NS_OK_UDEC_MOREOUTPUT) {
              goto error1;
           } else if(NS_FAILED(rv)) {
              goto error2;
           }

           dest += aLen;
        } else {
           if(dest+2 >= destEnd)
              goto error1;
           *dest++ = (PRUnichar) mData;
           *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;
        }
        mState = eState_CNS11643_1;
        break;

      case eState_ESC_24_29_G_SO_SI: // ESC $ ) G SO SI
        if(SO == *src) {
           mState = eState_CNS11643_1;
        } else if(ESC == *src) {
           mState = eState_ESC;
        } else {
           if(dest+1 >= destEnd)
              goto error1;
           *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;

           mState = eState_ESC_24_29_G_SO_SI;
        }
        break;

      case eState_ESC_24_2A: // ESC $ *
        if('H' == *src) {
           mState = eState_ESC_24_2A_H;
        } else {
           if(dest+4 >= destEnd)
              goto error1;
           *dest++ = (PRUnichar) ESC;
           *dest++ = (PRUnichar) '$';
           *dest++ = (PRUnichar) '*';
           *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;

           mState = eState_ASCII;
        }
        break;

      case eState_ESC_24_2A_H:  // ESC $ * H
        if(ESC == *src) {
           mState = eState_ESC_24_2A_H_ESC;
        } else {
           if(dest+5 >= destEnd)
              goto error1;
           *dest++ = (PRUnichar) ESC;
           *dest++ = (PRUnichar) '$';
           *dest++ = (PRUnichar) '*';
           *dest++ = (PRUnichar) 'H';
           *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;

           mState = eState_ASCII;
        } 
        break;

      case eState_ESC_24_2A_H_ESC:  // ESC $ * H ESC
        if(SS2 == *src) {
           mState = eState_CNS11643_2;
        } else if('$' == *src) {
           mState = eState_ESC_24;
        } else {
           if(dest+6 >= destEnd)
              goto error1;
           *dest++ = (PRUnichar) ESC;
           *dest++ = (PRUnichar) '$';
           *dest++ = (PRUnichar) '*';
           *dest++ = (PRUnichar) 'H';
           *dest++ = (PRUnichar) ESC;
           *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;

           mState = eState_ASCII;
        }
        break;

      case eState_CNS11643_2:  // ESC $ * H ESC SS2
        if(SI == *src) { // Shift-In (SI)
           mState = eState_ESC_24_2A_H_ESC_SS2_SI;
        } else if(ESC == *src) {
           mState = eState_ESC_24_2A_H_ESC;
        } else {
           if(0x20 < *src && *src < 0x7f) {
              mData = *src;
              mState = eState_CNS11643_2_2ndbyte;
           } else {
              if(dest+1 >= destEnd)
                 goto error1;
              *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;
           }
        }
        break;

      case eState_CNS11643_2_2ndbyte:   // ESC $ * H ESC SS2
        if(0x20 < *src && *src < 0x7f) {
           unsigned char cns[4];
           PRInt32 cnsLen = 4;
 
           cns[0] = (unsigned char) MBYTE;
           cns[1] = (unsigned char) (PMASK + 2);
           cns[2] = mData | 0x80;
           cns[3] = *src | 0x80;
 
           aLen = destEnd - dest;
           rv = EUCTW_To_Unicode(cns, cnsLen, dest, &aLen);
           if(rv == NS_OK_UDEC_MOREOUTPUT) {
              goto error1;
           } else if(NS_FAILED(rv)) {
              goto error2;
           }

           dest += aLen;
        } else {
           if(dest+2 >= destEnd)
              goto error1;
           *dest++ = (PRUnichar) mData;
           *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;
        }
        mState = eState_CNS11643_2;
        break;

      case eState_ESC_24_2A_H_ESC_SS2_SI:  // ESC $ * H ESC SS2 SI
        if(ESC == *src) {
           mState = eState_ESC_24_2A_H_ESC_SS2_SI_ESC;
        } else {
           if(dest+1 >= destEnd)
              goto error1;
           *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;

           mState = eState_ESC_24_2A_H_ESC_SS2_SI;
        }
        break;

      case eState_ESC_24_2A_H_ESC_SS2_SI_ESC:  // ESC $ * H ESC SS2 SI ESC
        if(SS2 == *src) {
           mState = eState_CNS11643_2;
        } else if('$' == *src) {
           mState = eState_ESC_24;
        } else {
           if(dest+1 >= destEnd)
              goto error1;
           *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;

           mState = eState_ESC_24_2A_H_ESC_SS2_SI;
        }
        break;

      case eState_ESC_24_2B: // ESC $ +
        if('I' <= *src && *src <= 'M') {
            mState = eState_ESC_24_2B_I;
            mPlaneID = *src - 'I' + 3;
        } else {
           if(dest+4 >= destEnd)
              goto error1;
           *dest++ = (PRUnichar) ESC;
           *dest++ = (PRUnichar) '$';
           *dest++ = (PRUnichar) '+';
           *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;

           mState = eState_ASCII;
        }
        break;

      case eState_ESC_24_2B_I:  // ESC $ + I
        if(ESC == *src) {
           mState = eState_ESC_24_2B_I_ESC;
        } else {
           if(dest+5 >= destEnd)
              goto error1;
           *dest++ = (PRUnichar) ESC;
           *dest++ = (PRUnichar) '$';
           *dest++ = (PRUnichar) '+';
           *dest++ = (PRUnichar) 'I' + mPlaneID - 3;
           *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;

           mState = eState_ASCII;
        }
        break;

      case eState_ESC_24_2B_I_ESC:  // ESC $ + I ESC
        if(SS3 == *src) {
           mState = eState_CNS11643_3;
        } else if('$' == *src) {
           mState = eState_ESC_24;
        } else {
           if(dest+6 >= destEnd)
              goto error1;
           *dest++ = (PRUnichar) ESC;
           *dest++ = (PRUnichar) '$';
           *dest++ = (PRUnichar) '+';
           *dest++ = (PRUnichar) 'I' + mPlaneID - 3;
           *dest++ = (PRUnichar) ESC;
           *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;

           mState = eState_ASCII;
        }
        break;

      case eState_CNS11643_3:   // ESC $ + I ESC SS3
        if(SI == *src) { // Shift-In (SI)
           mState = eState_ESC_24_2B_I_ESC_SS3_SI;
        } else if(ESC == *src) {
           mState = eState_ESC_24_2B_I_ESC;
        } else {
           if(0x20 < *src && *src < 0x7f) {
              mData = *src;
              mState = eState_CNS11643_3_2ndbyte;
           } else {
              if(dest+1 >= destEnd)
                 goto error1;
              *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;
           }
        }

        break;

      case eState_CNS11643_3_2ndbyte:  // ESC $ + I ESC SS3
        if(0x20 < *src && *src < 0x7f) {
           unsigned char cns[4];
           PRInt32 cnsLen = 4;

           cns[0] = (unsigned char) MBYTE;
           cns[1] = (unsigned char) (PMASK + mPlaneID);
           cns[2] = mData | 0x80;
           cns[3] = *src | 0x80;

           aLen = destEnd - dest;
           rv = EUCTW_To_Unicode(cns, cnsLen, dest, &aLen);
           if(rv == NS_OK_UDEC_MOREOUTPUT) {
              goto error1;
           } else if(NS_FAILED(rv)) {
              goto error2;
           }

           dest += aLen;
        } else {
           if(dest+2 >= destEnd)
              goto error1;
           *dest++ = (PRUnichar) mData;
           *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;
        }
        mState = eState_CNS11643_3;
        break;

      case eState_ESC_24_2B_I_ESC_SS3_SI:  // ESC $ + I ESC SS3 SI
        if(ESC == *src) {
           mState = eState_ESC_24_2B_I_ESC_SS3_SI_ESC;
        } else {
           if(dest+1 >= destEnd)
              goto error1;
           *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;

           mState = eState_ESC_24_2B_I_ESC_SS3_SI;
        }
        break;

      case eState_ESC_24_2B_I_ESC_SS3_SI_ESC:  // ESC $ + I ESC SS3 SI ESC
        if(SS3 == *src) {
           mState = eState_CNS11643_3;
        } else if('$' == *src) {
           mState = eState_ESC_24;
        } else {
           if(dest+1 >= destEnd)
              goto error1;
           *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;

           mState = eState_ESC_24_2B_I_ESC_SS3_SI;
        }
        break;

    } // switch
    src++;
  }

  *aDestLen = dest- aDest;
  return NS_OK;

error1:
  *aDestLen = dest-aDest;
  src++;
  if ((mState == eState_ASCII) && (src == srcEnd)) {
    return NS_OK;
  }
  *aSrcLen = src - (const unsigned char*)aSrc;
  return NS_OK_UDEC_MOREOUTPUT;

error2:
  *aSrcLen = src - (const unsigned char*)aSrc;
  *aDestLen = dest-aDest;
  mState = eState_ASCII;
  return NS_ERROR_UNEXPECTED;
}
