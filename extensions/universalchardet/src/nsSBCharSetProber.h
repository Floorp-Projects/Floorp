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
 * Contributor(s):
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
#ifndef nsSingleByteCharSetProber_h__
#define nsSingleByteCharSetProber_h__

#include "nsCharSetProber.h"

#define SAMPLE_SIZE 64
#define SB_ENOUGH_REL_THRESHOLD  1024
#define POSITIVE_SHORTCUT_THRESHOLD  (float)0.95
#define NEGATIVE_SHORTCUT_THRESHOLD  (float)0.05
#define SYMBOL_CAT_ORDER  250

typedef struct
{
  unsigned char *charToOrderMap;     //[256] table use to find a char's order
  char *precedenceMatrix;           //[SAMPLE_SIZE][SAMPLE_SIZE]; table to find a 2-char sequence's frequency
  PRBool keepEnglishLetter;         //it says if this script contains latin letters
  const char* charsetName;
} SequenceModel;


class nsSingleByteCharSetProber : public nsCharSetProber{
public:
  nsSingleByteCharSetProber(SequenceModel *model){mModel = model; Reset();};
  const char* GetCharSetName() {return mModel->charsetName;};
  nsProbingState HandleData(const char* aBuf, PRUint32 aLen);
  nsProbingState GetState(void) {return mState;};
  void      Reset(void);
  float     GetConfidence(void);
  void      SetOpion() {};
  PRBool KeepEnglishLetters() {return mModel->keepEnglishLetter;};

protected:
  nsProbingState mState;
  SequenceModel *mModel;

  //char order of last character
  unsigned char mLastOrder;

  PRUint32 mTotalSeqs;
  PRUint32 mNegativeSeqs;

  PRUint32 mTotalChar;
  //characters that fall in our sampling range
  PRUint32 mFreqChar;
};


extern SequenceModel Koi8rModel;
extern SequenceModel Win1251Model;
extern SequenceModel Latin5Model;
extern SequenceModel MacCyrillicModel;
extern SequenceModel Ibm866Model;
extern SequenceModel Ibm855Model;
extern SequenceModel Latin7Model;
extern SequenceModel Win1253Model;
extern SequenceModel Latin5BulgarianModel;
extern SequenceModel Win1251BulgarianModel;
extern SequenceModel Latin2HungarianModel;
extern SequenceModel Win1250HungarianModel;

#endif /* nsSingleByteCharSetProber_h__ */

