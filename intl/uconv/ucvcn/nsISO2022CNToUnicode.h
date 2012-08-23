/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsISO2022CNToUnicode_h__
#define nsISO2022CNToUnicode_h__
#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsUCSupport.h"

#define MBYTE       0x8e
#undef PMASK
#define PMASK       0xa0

#define SI          0x0f 
#define SO          0x0e
#define ESC         0x1b
#define SS2         0x4e
#define SS3         0x4f

class nsISO2022CNToUnicode : public nsBasicDecoderSupport
{
public:
  nsISO2022CNToUnicode() : 
        mState(eState_ASCII), 
        mPlaneID(0),
        mRunLength(0) { }

  virtual ~nsISO2022CNToUnicode() {}

  NS_IMETHOD Convert(const char *aSrc, int32_t * aSrcLength,
     PRUnichar * aDest, int32_t * aDestLength) ;

  NS_IMETHOD GetMaxLength(const char * aSrc, int32_t aSrcLength,
     int32_t * aDestLength)
  {
    *aDestLength = aSrcLength;
    return NS_OK;
  }

  NS_IMETHOD Reset()
  {
    mState = eState_ASCII;
    mPlaneID = 0;
    mRunLength = 0;

    return NS_OK;
  }

private:
  // State Machine ID
  enum {
    eState_ASCII,
    eState_ESC,                           // ESC
    eState_ESC_24,                        // ESC $

    eState_ESC_24_29,                     // ESC $ )
    eState_ESC_24_29_A,                   // ESC $ ) A
    eState_GB2312_1980,                   // ESC $ ) A SO
    eState_GB2312_1980_2ndbyte,           // ESC $ ) A SO
    eState_ESC_24_29_A_SO_SI,             // ESC $ ) A SO SI
    eState_ESC_24_29_G,                   // ESC $ ) G or H
    eState_CNS11643_1,                    // ESC $ ) G SO
    eState_CNS11643_1_2ndbyte,            // ESC $ ) G SO
    eState_ESC_24_29_G_SO_SI,             // ESC $ ) G SO SI

    eState_ESC_24_2A,                     // ESC $ *
    eState_ESC_24_2A_H,                   // ESC $ * H
    eState_ESC_24_2A_H_ESC,               // ESC $ * H ESC
    eState_CNS11643_2,                    // ESC $ * H ESC SS2
    eState_CNS11643_2_2ndbyte,            // ESC $ * H ESC SS2
    eState_ESC_24_2A_H_ESC_SS2_SI,        // ESC $ * H ESC SS2 SI
    eState_ESC_24_2A_H_ESC_SS2_SI_ESC,    // ESC $ * H ESC SS2 SI ESC

    eState_ESC_24_2B,                     // ESC $ +
    eState_ESC_24_2B_I,                   // ESC $ + I
    eState_ESC_24_2B_I_ESC,               // ESC $ + I ESC
    eState_CNS11643_3,                    // ESC $ + I ESC SS3
    eState_CNS11643_3_2ndbyte,            // ESC $ + I ESC SS3
    eState_ESC_24_2B_I_ESC_SS3_SI,        // ESC $ + I ESC SI
    eState_ESC_24_2B_I_ESC_SS3_SI_ESC,    // ESC $ + I ESC SI ESC
    eState_ERROR
  } mState;

  char mData;

  // Plane number for CNS11643 code
  int mPlaneID;

  // Length of non-ASCII run
  uint32_t mRunLength;

  // Decoder handler
  nsCOMPtr<nsIUnicodeDecoder> mGB2312_Decoder;
  nsCOMPtr<nsIUnicodeDecoder> mEUCTW_Decoder;

  NS_IMETHOD GB2312_To_Unicode(unsigned char *aSrc, int32_t aSrcLength,
     PRUnichar * aDest, int32_t * aDestLength) ;

  NS_IMETHOD EUCTW_To_Unicode(unsigned char *aSrc, int32_t aSrcLength,
     PRUnichar * aDest, int32_t * aDestLength) ;
};
#endif // nsISO2022CNToUnicode_h__
