/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
#ifndef nsTextTransformer_h___
#define nsTextTransformer_h___

#include "nsTextFragment.h"
#include "nsISupports.h"
#include "nsIPresContext.h"

class nsIContent;
class nsIFrame;
class nsILineBreaker;
class nsIWordBreaker;

// XXX I'm sure there are other special characters
#define CH_NBSP 160
#define CH_ENSP 8194		//<!ENTITY ensp    CDATA "&#8194;" -- en space, U+2002 ISOpub -->
#define CH_EMSP 8195		//<!ENTITY emsp    CDATA "&#8195;" -- em space, U+2003 ISOpub -->
#define CH_THINSP 8291	//<!ENTITY thinsp  CDATA "&#8201;" -- thin space, U+2009 ISOpub -->
#define CH_ZWNJ	8204	//<!ENTITY zwnj    CDATA "&#8204;" -- zero width non-joiner, U+200C NEW RFC 2070#define CH_SHY  173
#define CH_SHY  173

#ifdef IBMBIDI
#define CH_ZWJ  8205  //<!ENTITY zwj     CDATA "&#8205;" -- zero width joiner, U+200D NEW RFC 2070 -->
#define CH_LRM  8206  //<!ENTITY lrm     CDATA "&#8206;" -- left-to-right mark, U+200E NEW RFC 2070 -->
#define CH_RLM  8207  //<!ENTITY rlm     CDATA "&#8207;" -- right-to-left mark, U+200F NEW RFC 2070 -->
#define CH_LRE  8234  //<!CDATA "&#8234;" -- left-to-right embedding, U+202A -->
#define CH_RLE  8235  //<!CDATA "&#8235;" -- right-to-left embedding, U+202B -->
#define CH_PDF  8236  //<!CDATA "&#8236;" -- pop directional format, U+202C -->
#define CH_LRO  8237  //<!CDATA "&#8237;" -- left-to-right override, U+202D -->
#define CH_RLO  8238  //<!CDATA "&#8238;" -- right-to-left override, U+202E -->

#define IS_BIDI_CONTROL(_ch) \
  (((_ch) >= CH_ZWNJ && (_ch) <= CH_RLM) \
  || ((_ch) >= CH_LRE && (_ch) <= CH_RLO))
#endif // IBMBIDI

#define NS_TEXT_TRANSFORMER_AUTO_WORD_BUF_SIZE 128 // used to be 256

// Indicates whether the transformed text should be left as ascii
#define NS_TEXT_TRANSFORMER_LEAVE_AS_ASCII					1

// If at any point during GetNextWord or GetPrevWord we
// run across a multibyte (> 127) unicode character.
#define NS_TEXT_TRANSFORMER_HAS_MULTIBYTE					2

// The text in the transform buffer is ascii
#define NS_TEXT_TRANSFORMER_TRANSFORMED_TEXT_IS_ASCII		4

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
 * <LI>ascii to Unicode (if requested)
 * <LI>discarded characters
 * <LI>conversion of &nbsp that is not part of whitespace into a space
 * <LI>tab and newline characters to space (normal text only)
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
                    nsIWordBreaker* aWordBreaker,
                    nsIPresContext* aPresContext);

  ~nsTextTransformer();

  /**
   * Initialize the text transform. Use GetNextWord() and GetPrevWord()
   * to iterate the text
   *
   * The default is to transform all text to Unicode; however, you can
   * specify that the text should be left as ascii if possible. Note that
   * we don't step the text down from Unicode to ascii (even if it doesn't
   * contain multibyte characters) so this only happens for text fragments
   * that contain 1-byte text.
   * XXX This is currently not implemented for GetPreviousWord()
   * @see TransformedTextIsAscii()
   */
  nsresult Init(nsIFrame* aFrame,
                nsIContent* aContent,
                PRInt32 aStartingOffset,
                PRBool aLeaveAsAscii = PR_FALSE);

  PRInt32 GetContentLength() const {
    return mFrag ? mFrag->GetLength() : 0;
  }

  /**
   * Iterates the next word in the text fragment.
   *
   * Returns a pointer to the word, the number of characters in the word, the
   * content length of the word, whether it is whitespace, and whether the
   * text was transformed (any of the transformations listed above). The content
   * length can be greater than the word length if whitespace compression occured
   * or if characters were discarded
   *
   * The default behavior is to reset the transform buffer to the beginning,
   * but you can choose to not reste it and buffer across multiple words
   */
  PRUnichar* GetNextWord(PRBool aInWord,
                         PRInt32* aWordLenResult,
                         PRInt32* aContentLenResult,
                         PRBool* aIsWhitespaceResult,
                         PRBool* aWasTransformed,
                         PRBool aResetTransformBuf = PR_TRUE,
                         PRBool aForLineBreak = PR_TRUE);

  PRUnichar* GetPrevWord(PRBool aInWord,
                         PRInt32* aWordLenResult,
                         PRInt32* aContentLenResult,
                         PRBool* aIsWhitespaceResult,
                         PRBool aForLineBreak = PR_TRUE);

  
  // Returns PR_TRUE if the LEAVE_AS_ASCII flag is set
  PRBool LeaveAsAscii() const {
      return (mFlags & NS_TEXT_TRANSFORMER_LEAVE_AS_ASCII) ? PR_TRUE : PR_FALSE;
  }

  // Returns PR_TRUE if any of the characters are multibyte (greater than 127)
  PRBool HasMultibyte() const {
      return (mFlags & NS_TEXT_TRANSFORMER_HAS_MULTIBYTE) ? PR_TRUE : PR_FALSE;
  }

  // Returns PR_TRUE if the text in the transform bufer is ascii (i.e., it
  // doesn't contain any multibyte characters)
  PRBool TransformedTextIsAscii() const {
      return (mFlags & NS_TEXT_TRANSFORMER_TRANSFORMED_TEXT_IS_ASCII) ? PR_TRUE : PR_FALSE;
  }

  // Set or clears the LEAVE_AS_ASCII bit
  void SetLeaveAsAscii(PRBool aValue) {
      aValue ? mFlags |= NS_TEXT_TRANSFORMER_LEAVE_AS_ASCII : 
               mFlags &= (~NS_TEXT_TRANSFORMER_LEAVE_AS_ASCII);
  }
      
  // Set or clears the NS_TEXT_TRANSFORMER_HAS_MULTIBYTE bit
  void SetHasMultibyte(PRBool aValue) {
      aValue ? mFlags |= NS_TEXT_TRANSFORMER_HAS_MULTIBYTE : 
               mFlags &= (~NS_TEXT_TRANSFORMER_HAS_MULTIBYTE);
  }

  // Set or clears the NS_TEXT_TRANSFORMER_TRANSFORMED_TEXT_IS_ASCII bit
  void SetTransformedTextIsAscii(PRBool aValue) {
      aValue ? mFlags |= NS_TEXT_TRANSFORMER_TRANSFORMED_TEXT_IS_ASCII : 
               mFlags &= (~NS_TEXT_TRANSFORMER_TRANSFORMED_TEXT_IS_ASCII);
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
  PRInt32 ScanNormalAsciiText_F(PRInt32* aWordLen,
                                PRBool*  aWasTransformed);
  PRInt32 ScanNormalAsciiText_F_ForWordBreak(PRInt32* aWordLen,
                                PRBool*  aWasTransformed);
  PRInt32 ScanNormalUnicodeText_F(PRBool aForLineBreak,
                                  PRInt32* aWordLen,
                                  PRBool*  aWasTransformed);
  PRInt32 ScanPreWrapWhiteSpace_F(PRInt32* aWordLen);
  PRInt32 ScanPreAsciiData_F(PRInt32* aWordLen,
                             PRBool*  aWasTransformed);
  PRInt32 ScanPreData_F(PRInt32* aWordLen,
                        PRBool*  aWasTransformed);

  // Helper methods for GetPrevWord (B == backwards)
  PRInt32 ScanNormalWhiteSpace_B();
  PRInt32 ScanNormalAsciiText_B(PRInt32* aWordLen);
  PRInt32 ScanNormalUnicodeText_B(PRBool aForLineBreak, PRInt32* aWordLen);
  PRInt32 ScanPreWrapWhiteSpace_B(PRInt32* aWordLen);
  PRInt32 ScanPreData_B(PRInt32* aWordLen);

  // Converts the current text in the transform buffer from ascii to
  // Unicode
  void ConvertTransformedTextToUnicode();
  
  void LanguageSpecificTransform(PRUnichar* aText, PRInt32 aLen,
                                 PRBool* aWasTransformed);

  // The text fragment that we are looking at
  const nsTextFragment* mFrag;

  // Our current offset into the text fragment
  PRInt32 mOffset;

  // The frame's white-space mode we are using to process text
  enum {
    eNormal,
    ePreformatted,
    ePreWrap
  } mMode;
  
  nsILineBreaker* mLineBreaker;  // [WEAK]

  nsIWordBreaker* mWordBreaker;  // [WEAK]

  nsLanguageSpecificTransformType mLanguageSpecificTransformType;

  // Buffer used to hold the transformed words from GetNextWord or
  // GetPrevWord
  nsAutoTextBuffer mTransformBuf;

  // Our current position within the buffer. Used when iterating the next
  // word, because we may be requested to buffer across multiple words
  PRInt32 mBufferPos;
  
  // The frame's text-transform state
  PRUint8 mTextTransform;

  // Flag for controling mLeaveAsAscii, mHasMultibyte, mTransformedTextIsAscii
  PRUint8 mFlags;

  // prefs used to configure the double-click word selection behavior
  static PRPackedBool sWordSelectPrefInited;            // have we read the prefs yet?
  static PRPackedBool sWordSelectStopAtPunctuation;     // should we stop at punctuation?

#ifdef DEBUG
  static void SelfTest(nsILineBreaker* aLineBreaker,
                       nsIWordBreaker* aWordBreaker,
                       nsIPresContext* aPresContext);

  nsresult Init2(const nsTextFragment* aFrag,
                 PRInt32 aStartingOffset,
                 PRUint8 aWhiteSpace,
                 PRUint8 aTextTransform);
#endif
};

#endif /* nsTextTransformer_h___ */
