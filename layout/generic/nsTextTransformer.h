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
#ifndef nsTextTransformer_h___
#define nsTextTransformer_h___

#include "nsTextFragment.h"
#include "nsISupports.h"

class nsIContent;
class nsIFrame;
class nsTextRun;
class nsILineBreaker;
class nsIWordBreaker;

// XXX I'm sure there are other special characters
#define CH_NBSP 160
#define CH_SHY  173

#define NS_TEXT_TRANSFORMER_AUTO_WORD_BUF_SIZE 256

// A growable text buffer that tries to avoid using malloc by having a
// builtin buffer. Ideally used as an automatic variable.
class nsAutoTextBuffer {
public:
  nsAutoTextBuffer();
  ~nsAutoTextBuffer();

  nsresult GrowBy(PRInt32 aAtLeast, PRBool aCopyToHead = PR_TRUE);

  nsresult GrowTo(PRInt32 aNewSize, PRBool aCopyToHead = PR_TRUE);

  PRUnichar* GetBuffer() { return mBuffer; }
  PRUnichar* GetBufferEnd() { return mBuffer + mBufferLen; }
  PRInt32 GetBufferLength() const { return mBufferLen; }

  PRUnichar* mBuffer;
  PRInt32 mBufferLen;
  PRUnichar mAutoBuffer[NS_TEXT_TRANSFORMER_AUTO_WORD_BUF_SIZE];
};

//----------------------------------------

/**
 * This object manages the transformation of text:
 *
 * <UL>
 * <LI>whitespace compression
 * <LI>capitalization
 * <LI>lowercasing
 * <LI>uppercasing
 * <LI>ascii to Unicode
 * <LI>discarded characters
 * <LI>conversion of &nbsp that is not part of whitespace into a space
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
  // Note: The text transformer does not hold a reference to the line
  // breaker and work breaker objects
  nsTextTransformer(nsILineBreaker* aLineBreaker,
                    nsIWordBreaker* aWordBreaker);

  ~nsTextTransformer();

  /**
   * Initialize the text transform. Use GetNextWord() and GetPrevWord()
   * to iterate the text
   */
  nsresult Init(nsIFrame* aFrame,
                nsIContent* aContent,
                PRInt32 aStartingOffset);

  PRInt32 GetContentLength() const {
    return mFrag ? mFrag->GetLength() : 0;
  }

  /**
   * Iterates the next word in the text fragment.
   *
   * Returns a pointer to the word, the number of characters in the word, the
   * content length of the word, and whether it is whitespace. The content
   * length can be greater than the word length if whitespace compression
   * occured or if characters were discarded
   *
   * The default behavior is to reset the transform buffer to the beginning,
   * but you can choose to not reste it and buffer across multiple words
   */
  PRUnichar* GetNextWord(PRBool aInWord,
                         PRInt32* aWordLenResult,
                         PRInt32* aContentLenResult,
                         PRBool* aIsWhitespaceResult,
                         PRBool aResetTransformBuf = PR_TRUE,
                         PRBool aForLineBreak = PR_TRUE);

  PRUnichar* GetPrevWord(PRBool aInWord,
                         PRInt32* aWordLenResult,
                         PRInt32* aContentLenResult,
                         PRBool* aIsWhitespaceResult,
                         PRBool aForLineBreak = PR_TRUE);

  PRBool HasMultibyte() const {
    return mHasMultibyte;
  }

  PRUnichar* GetWordBuffer() {
    return mTransformBuf.GetBuffer();
  }

  PRInt32 GetWordBufferLength() const {
    return mTransformBuf.GetBufferLength();
  }

  static nsresult Initialize();

  static void Shutdown();

protected:
  // Helper methods for GetNextWord (F == forwards)
  PRInt32 ScanNormalWhiteSpace_F();
  PRInt32 ScanNormalAsciiText_F(PRInt32* aWordLen);
  PRInt32 ScanNormalUnicodeText_F(PRBool aForLineBreak, PRInt32* aWordLen);
  PRInt32 ScanPreWrapWhiteSpace_F(PRInt32* aWordLen);
  PRInt32 ScanPreAsciiData_F(PRInt32* aWordLen);
  PRInt32 ScanPreData_F(PRInt32* aWordLen);

  // Helper methods for GetPrevWord (B == backwards)
  PRInt32 ScanNormalWhiteSpace_B();
  PRInt32 ScanNormalAsciiText_B(PRInt32* aWordLen);
  PRInt32 ScanNormalUnicodeText_B(PRBool aForLineBreak, PRInt32* aWordLen);
  PRInt32 ScanPreWrapWhiteSpace_B(PRInt32* aWordLen);
  PRInt32 ScanPreData_B(PRInt32* aWordLen);

  // Set to true if at any point during GetNextWord or GetPrevWord we
  // run across a multibyte (> 127) unicode character.
  PRBool mHasMultibyte;

  // The text fragment that we are looking at
  const nsTextFragment* mFrag;

  // Our current offset into the text fragment
  PRInt32 mOffset;

  // The frame's text-transform state
  PRUint8 mTextTransform;

  // The frame's white-space mode we are using to process text
  enum {
    eNormal,
    ePreformatted,
    ePreWrap
  } mMode;

  nsILineBreaker* mLineBreaker;  // [WEAK]

  nsIWordBreaker* mWordBreaker;  // [WEAK]

  // Buffer used to hold the transformed words from GetNextWord or
  // GetPrevWord
  nsAutoTextBuffer mTransformBuf;

  // Our current position within the buffer. Used when iterating the next
  // word, because we may be requested to buffer across multiple words
  PRInt32 mBufferPos;

#ifdef DEBUG
  static void SelfTest(nsILineBreaker* aLineBreaker,
                       nsIWordBreaker* aWordBreaker);

  nsresult Init2(const nsTextFragment* aFrag,
                 PRInt32 aStartingOffset,
                 PRUint8 aWhiteSpace,
                 PRUint8 aTextTransform);
#endif
};

#endif /* nsTextTransformer_h___ */
