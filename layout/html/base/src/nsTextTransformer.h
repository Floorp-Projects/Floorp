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
  nsTextTransformer(PRUnichar* aBuffer, PRInt32 aBufLen);
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

  PRUnichar* GetNextWord(PRInt32& aWordLenResult,
                         PRInt32& aContentLenResult,
                         PRBool& aIsWhitespaceResult);

  PRUnichar* GetNextSection(PRInt32& aLineLenResult,
                            PRInt32& aContentLenResult);

  PRBool HasMultibyte() const {
    return mHasMultibyte;
  }

  PRUnichar* GetTextAt(PRInt32 aOffset);

protected:

  PRUnichar* mAutoBuffer;
  PRInt32 mAutoBufferLength;
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

  // XXX temporary
  PRBool mCompressWS;

  PRBool GrowBuffer();

#if 0
  // For each piece of content in the text-run we keep track of this
  // state.
  struct Text {
    nsIContent* mContent;
    nsTransformedTextFragment* mFrags;
    PRInt32 mNumFrags;
    Text* mNext;
  };

  Text* mTextList;

  void ReleaseResources();
#endif

  // A little state object used to do bookeeping between the low level
  // transform calls.
  struct TransformerState {
    PRBool mSkipLeadingWS;
    PRInt32 mContentOffset;
  };

  nsresult Transform2b(TransformerState& aState,
                       const nsTextFragment& frag);

  nsresult Transform1b(TransformerState& aState,
                       const nsTextFragment& frag);

  nsresult CompressWS2b(TransformerState& aState,
                        const nsTextFragment& frag);

  nsresult CompressWS1b(TransformerState& aState,
                        const nsTextFragment& frag);

  nsresult CompressWSAndTransform2b(TransformerState& aState,
                                    const nsTextFragment& frag);

  nsresult CompressWSAndTransform1b(TransformerState& aState,
                                    const nsTextFragment& frag);
};

#endif /* nsTextTransformer_h___ */
