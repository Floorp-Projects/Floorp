/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#ifndef nsTextTransformer_h___
#define nsTextTransformer_h___

#include "nsTextFragment.h"
#include "nsISupports.h"

class nsIFrame;
class nsTextRun;
class nsILineBreaker;

/**
 * This object manages the transformation of text:
 *
 * <UL>
 * <LI>whitespace compression
 * <LI>capitalization
 * <LI>lowercasing
 * <LI>uppercasing
 * </UL>
 *
 * Note that no transformations are applied that would impact word
 * breaking (like mapping &nbsp; into space, for example). In
 * addition, this logic will not strip leading or trailing whitespace
 * (across the entire run of text; leading whitespace can be skipped
 * for a frames text because of whitespace compression).
 */
class nsTextTransformer {
public:

  nsTextTransformer(PRUnichar* aBuffer, PRInt32 aBufLen, nsILineBreaker* aLineBreaker);

  ~nsTextTransformer();

  /**
   * Initialize the text transform. This is when the transformation
   * occurs. Subsequent calls to GetTransformedTextFor will just
   * return the result of the single transformation.
   */
  nsresult Init(/*nsTextRun& aTextRun, XXX*/
                nsIFrame* aFrame,
                PRInt32 aStartingOffset);

  PRInt32 GetContentLength() const {
    return mContentLength;
  }

  PRUnichar* GetNextWord(PRBool aInWord,
                         PRInt32& aWordLenResult,
                         PRInt32& aContentLenResult,
                         PRBool& aIsWhitespaceResult);

  PRUnichar* GetPrevWord(PRBool aInWord,
                               PRInt32& aWordLenResult,
                               PRInt32& aContentLenResult,
                               PRBool& aIsWhitespaceResult);
  PRBool HasMultibyte() const {
    return mHasMultibyte;
  }

  PRUnichar* GetTextAt(PRInt32 aOffset);

protected:
  PRBool GrowBuffer();

  PRUnichar* mAutoBuffer;
  PRUnichar* mBuffer;
  PRInt32 mBufferLength;
  PRBool mHasMultibyte;

  PRInt32 mContentLength;
  PRInt32 mStartingOffset;
  PRInt32 mOffset;

  const nsTextFragment* mFrags;
  PRInt32 mNumFrags;
  const nsTextFragment* mCurrentFrag;
  PRInt32 mCurrentFragOffset;

  PRUint8 mTextTransform;
  PRUint8 mPreformatted;

  nsILineBreaker* mLineBreaker;
};

#endif /* nsTextTransformer_h___ */
