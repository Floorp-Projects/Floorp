/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsSingleByteCharSetProber_h__
#define nsSingleByteCharSetProber_h__

#include "nsCharSetProber.h"

#define SAMPLE_SIZE 64
#define SB_ENOUGH_REL_THRESHOLD  1024
#define POSITIVE_SHORTCUT_THRESHOLD  (float)0.95
#define NEGATIVE_SHORTCUT_THRESHOLD  (float)0.05
#define SYMBOL_CAT_ORDER  250
#define NUMBER_OF_SEQ_CAT 4
#define POSITIVE_CAT   (NUMBER_OF_SEQ_CAT-1)
#define NEGATIVE_CAT   0

typedef struct
{
  const unsigned char* const charToOrderMap;    // [256] table use to find a char's order
  const PRUint8* const precedenceMatrix;  // [SAMPLE_SIZE][SAMPLE_SIZE]; table to find a 2-char sequence's frequency
  float  mTypicalPositiveRatio;     // = freqSeqs / totalSeqs 
  bool keepEnglishLetter;         // says if this script contains English characters (not implemented)
  const char* const charsetName;
} SequenceModel;


class nsSingleByteCharSetProber : public nsCharSetProber{
public:
  nsSingleByteCharSetProber(const SequenceModel *model) 
    :mModel(model), mReversed(false), mNameProber(0) { Reset(); }
  nsSingleByteCharSetProber(const SequenceModel *model, bool reversed, nsCharSetProber* nameProber)
    :mModel(model), mReversed(reversed), mNameProber(nameProber) { Reset(); }

  virtual const char* GetCharSetName();
  virtual nsProbingState HandleData(const char* aBuf, PRUint32 aLen);
  virtual nsProbingState GetState(void) {return mState;}
  virtual void      Reset(void);
  virtual float     GetConfidence(void);
  
  // This feature is not implemented yet. any current language model
  // contain this parameter as false. No one is looking at this
  // parameter or calling this method.
  // Moreover, the nsSBCSGroupProber which calls the HandleData of this
  // prober has a hard-coded call to FilterWithoutEnglishLetters which gets rid
  // of the English letters.
  bool KeepEnglishLetters() {return mModel->keepEnglishLetter;} // (not implemented)

#ifdef DEBUG_chardet
  virtual void  DumpStatus();
#endif

protected:
  nsProbingState mState;
  const SequenceModel* const mModel;
  const bool mReversed; // true if we need to reverse every pair in the model lookup

  //char order of last character
  unsigned char mLastOrder;

  PRUint32 mTotalSeqs;
  PRUint32 mSeqCounters[NUMBER_OF_SEQ_CAT];

  PRUint32 mTotalChar;
  //characters that fall in our sampling range
  PRUint32 mFreqChar;
  
  // Optional auxiliary prober for name decision. created and destroyed by the GroupProber
  nsCharSetProber* mNameProber; 

};


extern const SequenceModel Koi8rModel;
extern const SequenceModel Win1251Model;
extern const SequenceModel Latin5Model;
extern const SequenceModel MacCyrillicModel;
extern const SequenceModel Ibm866Model;
extern const SequenceModel Ibm855Model;
extern const SequenceModel Latin7Model;
extern const SequenceModel Win1253Model;
extern const SequenceModel Latin5BulgarianModel;
extern const SequenceModel Win1251BulgarianModel;
extern const SequenceModel Latin2HungarianModel;
extern const SequenceModel Win1250HungarianModel;
extern const SequenceModel Win1255Model;
extern const SequenceModel TIS620ThaiModel;

#endif /* nsSingleByteCharSetProber_h__ */

