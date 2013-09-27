/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsISO2022CNToUnicode.h"
#include "nsUCSupport.h"
#include "nsICharsetConverterManager.h"
#include "nsServiceManagerUtils.h"

static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

NS_IMETHODIMP nsISO2022CNToUnicode::GB2312_To_Unicode(unsigned char *aSrc, int32_t aSrcLength, PRUnichar * aDest, int32_t * aDestLength)
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

NS_IMETHODIMP nsISO2022CNToUnicode::EUCTW_To_Unicode(unsigned char *aSrc, int32_t aSrcLength, PRUnichar * aDest, int32_t * aDestLength)
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

NS_IMETHODIMP nsISO2022CNToUnicode::Convert(const char * aSrc, int32_t * aSrcLen, PRUnichar * aDest, int32_t * aDestLen)
{
  const unsigned char * srcEnd = (unsigned char *)aSrc + *aSrcLen;
  const unsigned char * src = (unsigned char *) aSrc;
  PRUnichar* destEnd = aDest + *aDestLen;
  PRUnichar* dest = aDest;
  nsresult rv;
  int32_t aLen; 

  while ((src < srcEnd))
  {
    switch (mState)
    {
      case eState_ASCII:
        if(ESC == *src) {
           mState = eState_ESC;
        } else {
           if (CHECK_OVERRUN(dest, destEnd, 1))
              goto error1;
           *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;

           mState = eState_ASCII;
        }
        break;

      case eState_ESC:    // ESC
        if('$' == *src) {
           mState = eState_ESC_24;
        } else {
           if (CHECK_OVERRUN(dest, destEnd, 2))
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
           if (CHECK_OVERRUN(dest, destEnd, 3))
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
           if (CHECK_OVERRUN(dest, destEnd, 4))
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
           mRunLength = 0;
        } else {
           if (CHECK_OVERRUN(dest, destEnd, 5))
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
           if (mRunLength == 0) {
              if (CHECK_OVERRUN(dest, destEnd, 1))
                 goto error1;
              *dest++ = 0xFFFD;
           }
           mRunLength = 0;
        } else if(ESC == *src) {
           mState = eState_ESC;
        } else {
           if(0x20 < *src && *src < 0x7f) {
              mData = *src;
              mState = eState_GB2312_1980_2ndbyte;
           } else {
              if (CHECK_OVERRUN(dest, destEnd, 1))
                 goto error1;
              *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;
           }
        }
        break; 

      case eState_GB2312_1980_2ndbyte:  // ESC $ ) A SO
        if(0x20 < *src && *src < 0x7f) {
           unsigned char gb[2];
           int32_t gbLen = 2;

           gb[0] = mData | 0x80;
           gb[1] = *src | 0x80;

           aLen = destEnd - dest;
           rv = GB2312_To_Unicode(gb, gbLen, dest, &aLen);
           ++mRunLength;
           if(rv == NS_OK_UDEC_MOREOUTPUT) {
              goto error1;
           } else if(NS_FAILED(rv)) {
              goto error2;
           }

           dest += aLen;
        } else {
           if (CHECK_OVERRUN(dest, destEnd, 2))
              goto error1;
           *dest++ = (PRUnichar) mData;
           *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;
        }
        mState = eState_GB2312_1980;
        break;

      case eState_ESC_24_29_A_SO_SI:  // ESC $ ) A SO SI
        if(SO == *src) {
           mState = eState_GB2312_1980;
           mRunLength = 0;
        } else if(ESC == *src) {
           mState = eState_ESC;
        } else {
           if (CHECK_OVERRUN(dest, destEnd, 1))
              goto error1;
           *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;

           mState = eState_ESC_24_29_A_SO_SI;
        }
        break;

      case eState_ESC_24_29_G:   // ESC $ ) G
        if(SO == *src) {
           mState = eState_CNS11643_1;
           mRunLength = 0;
        } else {
           if (CHECK_OVERRUN(dest, destEnd, 5))
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
           if (mRunLength == 0) {
              if (CHECK_OVERRUN(dest, destEnd, 1))
                 goto error1;
              *dest++ = 0xFFFD;
           }
           mRunLength = 0;
        } else if(ESC == *src) {
           mState = eState_ESC;
        } else {
           if(0x20 < *src && *src < 0x7f) {
              mData = *src;
              mState = eState_CNS11643_1_2ndbyte;
           } else {
              if (CHECK_OVERRUN(dest, destEnd, 1))
                 goto error1;
              *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;
           }
        }
        break;

      case eState_CNS11643_1_2ndbyte:  // ESC $ ) G SO
        if(0x20 < *src && *src < 0x7f) {
           unsigned char cns[4];
           int32_t cnsLen = 2;

           cns[0] = mData | 0x80;
           cns[1] = *src | 0x80;

           aLen = destEnd - dest;
           rv = EUCTW_To_Unicode(cns, cnsLen, dest, &aLen);
           ++mRunLength;
           if(rv == NS_OK_UDEC_MOREOUTPUT) {
              goto error1;
           } else if(NS_FAILED(rv)) {
              goto error2;
           }

           dest += aLen;
        } else {
           if (CHECK_OVERRUN(dest, destEnd, 2))
              goto error1;
           *dest++ = (PRUnichar) mData;
           *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;
        }
        mState = eState_CNS11643_1;
        break;

      case eState_ESC_24_29_G_SO_SI: // ESC $ ) G SO SI
        if(SO == *src) {
           mState = eState_CNS11643_1;
           mRunLength = 0;
        } else if(ESC == *src) {
           mState = eState_ESC;
        } else {
           if (CHECK_OVERRUN(dest, destEnd, 1))
              goto error1;
           *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;

           mState = eState_ESC_24_29_G_SO_SI;
        }
        break;

      case eState_ESC_24_2A: // ESC $ *
        if('H' == *src) {
           mState = eState_ESC_24_2A_H;
        } else {
           if (CHECK_OVERRUN(dest, destEnd, 4))
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
           if (CHECK_OVERRUN(dest, destEnd, 5))
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
           mRunLength = 0;
        } else if('$' == *src) {
           mState = eState_ESC_24;
        } else {
           if (CHECK_OVERRUN(dest, destEnd, 6))
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
           if (mRunLength == 0) {
              if (CHECK_OVERRUN(dest, destEnd, 1))
                 goto error1;
              *dest++ = 0xFFFD;
           }
           mRunLength = 0;
        } else if(ESC == *src) {
           mState = eState_ESC_24_2A_H_ESC;
        } else {
           if(0x20 < *src && *src < 0x7f) {
              mData = *src;
              mState = eState_CNS11643_2_2ndbyte;
           } else {
              if (CHECK_OVERRUN(dest, destEnd, 1))
                 goto error1;
              *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;
           }
        }
        break;

      case eState_CNS11643_2_2ndbyte:   // ESC $ * H ESC SS2
        if(0x20 < *src && *src < 0x7f) {
           unsigned char cns[4];
           int32_t cnsLen = 4;
 
           cns[0] = (unsigned char) MBYTE;
           cns[1] = (unsigned char) (PMASK + 2);
           cns[2] = mData | 0x80;
           cns[3] = *src | 0x80;
 
           aLen = destEnd - dest;
           rv = EUCTW_To_Unicode(cns, cnsLen, dest, &aLen);
           ++mRunLength;
           if(rv == NS_OK_UDEC_MOREOUTPUT) {
              goto error1;
           } else if(NS_FAILED(rv)) {
              goto error2;
           }

           dest += aLen;
        } else {
           if (CHECK_OVERRUN(dest, destEnd, 2))
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
           if (CHECK_OVERRUN(dest, destEnd, 1))
              goto error1;
           *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;

           mState = eState_ESC_24_2A_H_ESC_SS2_SI;
        }
        break;

      case eState_ESC_24_2A_H_ESC_SS2_SI_ESC:  // ESC $ * H ESC SS2 SI ESC
        if(SS2 == *src) {
           mState = eState_CNS11643_2;
           mRunLength = 0;
        } else if('$' == *src) {
           mState = eState_ESC_24;
        } else {
           if (CHECK_OVERRUN(dest, destEnd, 1))
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
           if (CHECK_OVERRUN(dest, destEnd, 4))
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
           if (CHECK_OVERRUN(dest, destEnd, 5))
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
           mRunLength = 0;
        } else if('$' == *src) {
           mState = eState_ESC_24;
        } else {
           if (CHECK_OVERRUN(dest, destEnd, 6))
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
           if (mRunLength == 0) {
              if (CHECK_OVERRUN(dest, destEnd, 1))
                 goto error1;
              *dest++ = 0xFFFD;
           }
           mRunLength = 0;
        } else if(ESC == *src) {
           mState = eState_ESC_24_2B_I_ESC;
        } else {
           if(0x20 < *src && *src < 0x7f) {
              mData = *src;
              mState = eState_CNS11643_3_2ndbyte;
           } else {
              if (CHECK_OVERRUN(dest, destEnd, 1))
                 goto error1;
              *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;
           }
        }

        break;

      case eState_CNS11643_3_2ndbyte:  // ESC $ + I ESC SS3
        if(0x20 < *src && *src < 0x7f) {
           unsigned char cns[4];
           int32_t cnsLen = 4;

           cns[0] = (unsigned char) MBYTE;
           cns[1] = (unsigned char) (PMASK + mPlaneID);
           cns[2] = mData | 0x80;
           cns[3] = *src | 0x80;

           aLen = destEnd - dest;
           rv = EUCTW_To_Unicode(cns, cnsLen, dest, &aLen);
           ++mRunLength;
           if(rv == NS_OK_UDEC_MOREOUTPUT) {
              goto error1;
           } else if(NS_FAILED(rv)) {
              goto error2;
           }

           dest += aLen;
        } else {
           if (CHECK_OVERRUN(dest, destEnd, 2))
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
           if (CHECK_OVERRUN(dest, destEnd, 1))
              goto error1;
           *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;

           mState = eState_ESC_24_2B_I_ESC_SS3_SI;
        }
        break;

      case eState_ESC_24_2B_I_ESC_SS3_SI_ESC:  // ESC $ + I ESC SS3 SI ESC
        if(SS3 == *src) {
           mState = eState_CNS11643_3;
           mRunLength = 0;
        } else if('$' == *src) {
           mState = eState_ESC_24;
        } else {
           if (CHECK_OVERRUN(dest, destEnd, 1))
              goto error1;
           *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;

           mState = eState_ESC_24_2B_I_ESC_SS3_SI;
        }
        break;

      case eState_ERROR:
        NS_NOTREACHED("unhandled case");
        goto error2;

    } // switch
    src++;
  }

  *aDestLen = dest- aDest;
  return NS_OK;

error1:
  *aDestLen = dest-aDest;
  *aSrcLen = src - (const unsigned char*)aSrc;
  return NS_OK_UDEC_MOREOUTPUT;

error2:
  *aSrcLen = src - (const unsigned char*)aSrc;
  *aDestLen = dest-aDest;
  mState = eState_ASCII;
  return NS_ERROR_UNEXPECTED;
}
