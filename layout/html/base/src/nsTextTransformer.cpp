/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include <ctype.h>
#include "nsCOMPtr.h"
#include "nsTextTransformer.h"
#include "nsContentUtils.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsITextContent.h"
#include "nsStyleConsts.h"
#include "nsILineBreaker.h"
#include "nsIWordBreaker.h"
#include "nsIServiceManager.h"
#include "nsUnicharUtilCIID.h"
#include "nsUnicharUtils.h"
#include "nsICaseConversion.h"
#include "prenv.h"
#ifdef IBMBIDI
#include "nsLayoutAtoms.h"
#endif


PRBool nsTextTransformer::sWordSelectListenerPrefChecked = PR_FALSE;
PRBool nsTextTransformer::sWordSelectStopAtPunctuation = PR_FALSE;
static const char kWordSelectPref[] = "layout.word_select.stop_at_punctuation";

// static
int
nsTextTransformer::WordSelectPrefCallback(const char* aPref, void* aClosure)
{
  sWordSelectStopAtPunctuation = nsContentUtils::GetBoolPref(kWordSelectPref);

  return 0;
}

nsAutoTextBuffer::nsAutoTextBuffer()
  : mBuffer(mAutoBuffer),
    mBufferLen(NS_TEXT_TRANSFORMER_AUTO_WORD_BUF_SIZE)
{
}

nsAutoTextBuffer::~nsAutoTextBuffer()
{
  if (mBuffer && (mBuffer != mAutoBuffer)) {
    delete [] mBuffer;
  }
}

nsresult
nsAutoTextBuffer::GrowBy(PRInt32 aAtLeast, PRBool aCopyToHead)
{
  PRInt32 newSize = mBufferLen * 2;
  if (newSize < mBufferLen + aAtLeast) {
    newSize = mBufferLen + aAtLeast + 100;
  }
  return GrowTo(newSize, aCopyToHead);
}

nsresult
nsAutoTextBuffer::GrowTo(PRInt32 aNewSize, PRBool aCopyToHead)
{
  if (aNewSize > mBufferLen) {
    PRUnichar* newBuffer = new PRUnichar[aNewSize];
    if (!newBuffer) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    memcpy(&newBuffer[aCopyToHead ? 0 : mBufferLen],
           mBuffer, sizeof(PRUnichar) * mBufferLen);
    if (mBuffer != mAutoBuffer) {
      delete [] mBuffer;
    }
    mBuffer = newBuffer;
    mBufferLen = aNewSize;
  }
  return NS_OK;
}

//----------------------------------------------------------------------

static NS_DEFINE_CID(kUnicharUtilCID, NS_UNICHARUTIL_CID);

static nsICaseConversion* gCaseConv =  nsnull;

nsresult
nsTextTransformer::Initialize()
{
  // read in our global word selection prefs
  if ( !sWordSelectListenerPrefChecked ) {
    sWordSelectListenerPrefChecked = PR_TRUE;

    sWordSelectStopAtPunctuation =
      nsContentUtils::GetBoolPref(kWordSelectPref);

    nsContentUtils::RegisterPrefCallback(kWordSelectPref,
                                         WordSelectPrefCallback, nsnull);
  }

  return NS_OK;
}
static nsresult EnsureCaseConv()
{
  nsresult res = NS_OK;
  if (!gCaseConv) {
    res = nsServiceManager::GetService(kUnicharUtilCID, NS_GET_IID(nsICaseConversion),
                                       (nsISupports**)&gCaseConv);
    NS_ASSERTION( NS_SUCCEEDED(res), "cannot get UnicharUtil");
    NS_ASSERTION( gCaseConv != NULL, "cannot get UnicharUtil");
  }
  return res;
}

void
nsTextTransformer::Shutdown()
{
  if (gCaseConv) {
    nsServiceManager::ReleaseService(kUnicharUtilCID, gCaseConv);
    gCaseConv = nsnull;
  }
}

// For now, we have only a couple of characters to strip out. If we get
// any more, change this to use a bitset to lookup into.
//   CH_SHY - soft hyphen (discretionary hyphen)
#ifdef IBMBIDI
// added BIDI formatting codes
#define IS_DISCARDED(_ch) \
  (((_ch) == CH_SHY) || ((_ch) == '\r') || IS_BIDI_CONTROL(_ch))
#else
#define IS_DISCARDED(_ch) \
  (((_ch) == CH_SHY) || ((_ch) == '\r'))
#endif


#define MAX_UNIBYTE 127

MOZ_DECL_CTOR_COUNTER(nsTextTransformer)

nsTextTransformer::nsTextTransformer(nsILineBreaker* aLineBreaker,
                                     nsIWordBreaker* aWordBreaker,
                                     nsPresContext* aPresContext)
  : mFrag(nsnull),
    mOffset(0),
    mMode(eNormal),
    mLineBreaker(aLineBreaker),
    mWordBreaker(aWordBreaker),
    mBufferPos(0),
    mTextTransform(NS_STYLE_TEXT_TRANSFORM_NONE),
    mFlags(0)
{
  MOZ_COUNT_CTOR(nsTextTransformer);

  mLanguageSpecificTransformType =
    aPresContext->LanguageSpecificTransformType();

#ifdef IBMBIDI
  mPresContext = aPresContext;
#endif
  if (aLineBreaker == nsnull && aWordBreaker == nsnull )
    NS_ASSERTION(0, "invalid creation of nsTextTransformer");
  
#ifdef DEBUG
  static PRBool firstTime = PR_TRUE;
  if (firstTime) {
    firstTime = PR_FALSE;
    SelfTest(aLineBreaker, aWordBreaker, aPresContext);
  }
#endif
}

nsTextTransformer::~nsTextTransformer()
{
  MOZ_COUNT_DTOR(nsTextTransformer);
}

nsresult
nsTextTransformer::Init(nsIFrame* aFrame,
                        nsIContent* aContent,
                        PRInt32 aStartingOffset,
                        PRBool aForceArabicShaping,
                        PRBool aLeaveAsAscii)
{
  /*
   * If the document has Bidi content, check whether we need to do
   * Arabic shaping.
   *
   *  Does the frame contains Arabic characters
   *   (mCharType == eCharType_RightToLeftArabic)?
   *  Are we rendering character by character (aForceArabicShaping ==
   *   PR_TRUE)? If so, we always do our own Arabic shaping, even if
   *   the platform has native shaping support. Otherwise, we only do
   *   shaping if the platform has no shaping support.
   *
   *  We do numeric shaping in all Bidi documents.
   */
  if (mPresContext->BidiEnabled()) {
    mCharType = (nsCharType)NS_PTR_TO_INT32(aFrame->GetProperty(nsLayoutAtoms::charType));
    if (mCharType == eCharType_RightToLeftArabic) {
      if (aForceArabicShaping) {
        SetNeedsArabicShaping(PR_TRUE);
      }
      else {
        if (!mPresContext->IsBidiSystem()) {
          SetNeedsArabicShaping(PR_TRUE);
        }
      }
    }
    SetNeedsNumericShaping(PR_TRUE);
  }

  // Get the contents text content
  nsresult rv;
  nsCOMPtr<nsITextContent> tc = do_QueryInterface(aContent, &rv);
  if (tc.get()) {
    mFrag = tc->Text();

    // Sanitize aStartingOffset
    if (aStartingOffset < 0) {
      NS_WARNING("bad starting offset");
      aStartingOffset = 0;
    }
    else if (aStartingOffset > mFrag->GetLength()) {
      NS_WARNING("bad starting offset");
      aStartingOffset = mFrag->GetLength();
    }
    mOffset = aStartingOffset;

    // Get the frames text style information
    const nsStyleText* styleText = aFrame->GetStyleText();
    if (NS_STYLE_WHITESPACE_PRE == styleText->mWhiteSpace) {
      mMode = ePreformatted;
    }
    else if (NS_STYLE_WHITESPACE_MOZ_PRE_WRAP == styleText->mWhiteSpace) {
      mMode = ePreWrap;
    }
    mTextTransform = styleText->mTextTransform;
    
    if (aLeaveAsAscii) { // See if the text fragment is 1-byte text
      SetLeaveAsAscii(PR_TRUE);	    
      // XXX Currently we only leave it as ascii for normal text and not for preformatted
      // or preformatted wrapped text or language specific transforms
      if (mFrag->Is2b() || (eNormal != mMode) ||
          (mLanguageSpecificTransformType !=
           eLanguageSpecificTransformType_None))
        // We don't step down from Unicode to ascii
        SetLeaveAsAscii(PR_FALSE);           
    } 
    else 
      SetLeaveAsAscii(PR_FALSE);
  }
  return rv;
}

//----------------------------------------------------------------------

// wordlen==1, contentlen=newOffset-currentOffset, isWhitespace=t
PRInt32
nsTextTransformer::ScanNormalWhiteSpace_F()
{
  const nsTextFragment* frag = mFrag;
  PRInt32 fragLen = frag->GetLength();
  PRInt32 offset = mOffset;

  for (; offset < fragLen; offset++) {
    PRUnichar ch = frag->CharAt(offset);
    if (!XP_IS_SPACE(ch)) {
      // If character is not discardable then stop looping, otherwise
      // let the discarded character collapse with the other spaces.
      if (!IS_DISCARDED(ch)) {
        break;
      }
    }
  }

  // Make sure we have enough room in the transform buffer
  if (mBufferPos >= mTransformBuf.mBufferLen) {
    mTransformBuf.GrowBy(128);
  }

  if (TransformedTextIsAscii()) {
    unsigned char*  bp = (unsigned char*)mTransformBuf.mBuffer;
    bp[mBufferPos++] = ' ';
  } else {
    mTransformBuf.mBuffer[mBufferPos++] = PRUnichar(' ');
  }
  return offset;
}
  
void
nsTextTransformer::ConvertTransformedTextToUnicode()
{
  // Go backwards over the characters and convert them.
  PRInt32         lastChar = mBufferPos - 1;
  unsigned char*  cp1 = (unsigned char*)mTransformBuf.mBuffer + lastChar;
  PRUnichar*      cp2 = mTransformBuf.mBuffer + lastChar;
  
  NS_ASSERTION(mTransformBuf.mBufferLen >= mBufferPos,
               "transform buffer is too small");
  for (PRInt32 count = mBufferPos; count > 0; count--) {
    *cp2-- = PRUnichar(*cp1--);
  }
}

// wordlen==*aWordLen, contentlen=newOffset-currentOffset, isWhitespace=f
PRInt32
nsTextTransformer::ScanNormalAsciiText_F(PRInt32* aWordLen,
                                         PRBool*  aWasTransformed)
{
  const nsTextFragment* frag = mFrag;
  PRInt32 fragLen = frag->GetLength();
  PRInt32 offset = mOffset;
  PRInt32 prevBufferPos = mBufferPos;
  const unsigned char* cp = (const unsigned char*)frag->Get1b() + offset;
  union {
    unsigned char* bp1;
    PRUnichar* bp2;
  };
  bp2 = mTransformBuf.GetBuffer();
  if (TransformedTextIsAscii()) {
    bp1 += mBufferPos;
  } else {
    bp2 += mBufferPos;
  }

  for (; offset < fragLen; offset++) {
    unsigned char ch = *cp++;
    if (XP_IS_SPACE(ch)) {
      break;
    }
    if (CH_NBSP == ch) {
      ch = ' ';
      *aWasTransformed = PR_TRUE;
    }
    else if (IS_DISCARDED(ch)) {
      // Strip discarded characters from the transformed output
      continue;
    }
    if (ch > MAX_UNIBYTE) {
      // The text has a multibyte character so we can no longer leave the
      // text as ascii text
      SetHasMultibyte(PR_TRUE);        
		
      if (TransformedTextIsAscii()) { 
        SetTransformedTextIsAscii(PR_FALSE);
        *aWasTransformed = PR_TRUE;

        // Transform any existing ascii text to Unicode
        if (mBufferPos > 0) {
          ConvertTransformedTextToUnicode();
          bp2 = mTransformBuf.GetBuffer() + mBufferPos;
        }
      }
    }
    if (mBufferPos >= mTransformBuf.mBufferLen) {
      nsresult rv = mTransformBuf.GrowBy(128);
      if (NS_FAILED(rv)) {
        // If we run out of space then just truncate the text
        break;
      }
      bp2 = mTransformBuf.GetBuffer();
      if (TransformedTextIsAscii()) {
        bp1 += mBufferPos;
      } else {
        bp2 += mBufferPos;
      }
    }
    if (TransformedTextIsAscii()) {
      *bp1++ = ch;
    } else {
      *bp2++ = PRUnichar(ch);
    }
    mBufferPos++;
  }

  *aWordLen = mBufferPos - prevBufferPos;
  return offset;
}

PRInt32
nsTextTransformer::ScanNormalAsciiText_F_ForWordBreak(PRInt32* aWordLen,
                                         PRBool*  aWasTransformed,
                                         PRBool aIsKeyboardSelect)
{
  const nsTextFragment* frag = mFrag;
  PRInt32 fragLen = frag->GetLength();
  PRInt32 offset = mOffset;
  PRInt32 prevBufferPos = mBufferPos;
  PRBool breakAfterThis = PR_FALSE;
  const unsigned char* cp = (const unsigned char*)frag->Get1b() + offset;
  union {
    unsigned char* bp1;
    PRUnichar* bp2;
  };
  bp2 = mTransformBuf.GetBuffer();
  if (TransformedTextIsAscii()) {
    bp1 += mBufferPos;
  } else {
    bp2 += mBufferPos;
  }
  PRBool readingAlphaNumeric = PR_TRUE; //only used in sWordSelectStopAtPunctuation

  // We must know if we are starting in alpha numerics.
  // Treat high bit chars as alphanumeric, otherwise we get stuck on accented letters
  // We can't trust isalnum() results for isalnum()
  // Therefore we don't stop at non-ascii (high bit) punctuation,
  // which is just fine. The punctuation we care about is low bit.
  if (sWordSelectStopAtPunctuation && offset < fragLen)
    readingAlphaNumeric = isalnum((unsigned char)*cp) || !IS_ASCII_CHAR(*cp);
  
  for (; offset < fragLen && !breakAfterThis; offset++) {
    unsigned char ch = *cp++;
    if (CH_NBSP == ch) {
      ch = ' ';
      *aWasTransformed = PR_TRUE;
      if (offset == mOffset)
        breakAfterThis = PR_TRUE;
      else
        break;
    }
    else if (XP_IS_SPACE(ch)) {
      break;
    }
    else if (sWordSelectStopAtPunctuation && 
             readingAlphaNumeric && !isalnum(ch) && IS_ASCII_CHAR(ch)) {
      if (!aIsKeyboardSelect)
        break;
      // For keyboard move-by-word, need to pass by at least
      // one alphanumeric char before stopping at punct
      readingAlphaNumeric = PR_FALSE;
    }
    else if (sWordSelectStopAtPunctuation && 
            !readingAlphaNumeric && (isalnum(ch) || !IS_ASCII_CHAR(ch))) {
      // On some platforms, punctuation breaks for word selection
      break;
    }
    else if (IS_DISCARDED(ch)) {
      // Strip discarded characters from the transformed output
      continue;
    }
    if (ch > MAX_UNIBYTE) {
      // The text has a multibyte character so we can no longer leave the
      // text as ascii text
      SetHasMultibyte(PR_TRUE);

      if (TransformedTextIsAscii()) {
        SetTransformedTextIsAscii(PR_FALSE);
        *aWasTransformed = PR_TRUE;

        // Transform any existing ascii text to Unicode
        if (mBufferPos > 0) {
          ConvertTransformedTextToUnicode();
          bp2 = mTransformBuf.GetBuffer() + mBufferPos;
        }
      }
    }
    if (mBufferPos >= mTransformBuf.mBufferLen) {
      nsresult rv = mTransformBuf.GrowBy(128);
      if (NS_FAILED(rv)) {
        // If we run out of space then just truncate the text
        break;
      }
      bp2 = mTransformBuf.GetBuffer();
      if (TransformedTextIsAscii()) {
        bp1 += mBufferPos;
      } else {
        bp2 += mBufferPos;
      }
    }
    if (TransformedTextIsAscii()) {
      *bp1++ = ch;
    } else {
      *bp2++ = PRUnichar(ch);
    }
    mBufferPos++;
  }

  *aWordLen = mBufferPos - prevBufferPos;
  return offset;
}


// wordlen==*aWordLen, contentlen=newOffset-currentOffset, isWhitespace=f
PRInt32
nsTextTransformer::ScanNormalUnicodeText_F(PRBool   aForLineBreak,
                                           PRInt32* aWordLen,
                                           PRBool*  aWasTransformed)
{
  const nsTextFragment* frag = mFrag;
  const PRUnichar* cp0 = frag->Get2b();
  PRInt32 fragLen = frag->GetLength();
#ifdef IBMBIDI
  if (*aWordLen > 0 && *aWordLen < fragLen) {
    fragLen = *aWordLen;
  }
#endif
  PRInt32 offset = mOffset;

  PRUnichar firstChar = frag->CharAt(offset++);

#ifdef IBMBIDI
  // Need to strip BIDI controls even when those are 'firstChars'.
  // This doesn't seem to produce bug 14280 (or similar bugs).
  while (offset < fragLen && IS_BIDI_CONTROL(firstChar) ) {
    firstChar = frag->CharAt(offset++);
  }
#endif // IBMBIDI

  if (firstChar > MAX_UNIBYTE) SetHasMultibyte(PR_TRUE);

  // Only evaluate complex breaking logic if there are more characters
  // beyond the first to look at.
  PRInt32 numChars = 1;
  if (offset < fragLen) {
    const PRUnichar* cp = cp0 + offset;
    PRBool breakBetween = PR_FALSE;
    if (aForLineBreak) {
      mLineBreaker->BreakInBetween(&firstChar, 1, cp, (fragLen-offset), &breakBetween);
    }
    else {
      mWordBreaker->BreakInBetween(&firstChar, 1, cp, (fragLen-offset), &breakBetween);
    }

    // don't transform the first character until after BreakInBetween is called
    // Kipp originally did this at the top of the function, which was too early.
    // see bug 14280
    if (CH_NBSP == firstChar) {
      firstChar = ' ';
      *aWasTransformed = PR_TRUE;
    }
    nsresult rv = mTransformBuf.GrowTo(mBufferPos + 1);
    if (NS_FAILED(rv)) {
		*aWordLen = 0;
		return offset - 1;
	}

    mTransformBuf.mBuffer[mBufferPos++] = firstChar;

    if (!breakBetween) {
      // Find next position
      PRBool tryNextFrag;
      PRUint32 next;
      if (aForLineBreak) {
        mLineBreaker->Next(cp0, fragLen, offset, &next, &tryNextFrag);
      }
      else {
        mWordBreaker->NextWord(cp0, fragLen, offset, &next, &tryNextFrag);
      }
      numChars = (PRInt32) (next - (PRUint32) offset) + 1;

      // Since we know the number of characters we're adding grow the buffer
      // now before we start copying
      nsresult rv = mTransformBuf.GrowTo(mBufferPos + numChars);
      if (NS_FAILED(rv)) {
        numChars = mTransformBuf.GetBufferLength() - mBufferPos;
      }

      offset += numChars - 1;

      // 1. convert nbsp into space
      // 2. check for discarded characters
      // 3. check mHasMultibyte flag
      // 4. copy buffer
      PRUnichar* bp = &mTransformBuf.mBuffer[mBufferPos];
      const PRUnichar* end = cp + numChars - 1;
      while (cp < end) {
        PRUnichar ch = *cp++;
        if (CH_NBSP == ch) {
          ch = ' ';
        }
        else if (IS_DISCARDED(ch) || (ch == 0x0a) || (ch == 0x0d)) {
          // Strip discarded characters from the transformed output
          numChars--;
          continue;
        }
        if (ch > MAX_UNIBYTE) SetHasMultibyte(PR_TRUE);
        *bp++ = ch;
        mBufferPos++;
      }
    }
  }
  else 
  { // transform the first character
    // we do this here, rather than at the top of the function (like Kipp originally had it)
    // because if we must call BreakInBetween, then we must do so before the transformation
    // this is the case where BreakInBetween does not need to be called at all.
    // see bug 14280
    if (CH_NBSP == firstChar) {
      firstChar = ' ';
      *aWasTransformed = PR_TRUE;
    }
    nsresult rv = mTransformBuf.GrowTo(mBufferPos + 1);
    if (NS_FAILED(rv)) {
		*aWordLen = 0;
		return offset - 1;
	}
    mTransformBuf.mBuffer[mBufferPos++] = firstChar;
  }

  *aWordLen = numChars;
  return offset;
}

// wordlen==*aWordLen, contentlen=newOffset-currentOffset, isWhitespace=t
PRInt32
nsTextTransformer::ScanPreWrapWhiteSpace_F(PRInt32* aWordLen)
{
  const nsTextFragment* frag = mFrag;
  PRInt32 fragLen = frag->GetLength();
  PRInt32 offset = mOffset;
  PRUnichar* bp = mTransformBuf.GetBuffer() + mBufferPos;
  PRUnichar* endbp = mTransformBuf.GetBufferEnd();
  PRInt32 prevBufferPos = mBufferPos;

  for (; offset < fragLen; offset++) {
    // This function is used for both Unicode and ascii strings so don't
    // make any assumptions about what kind of data it is
    PRUnichar ch = frag->CharAt(offset);
    if (!XP_IS_SPACE(ch) || (ch == '\t') || (ch == '\n')) {
      if (IS_DISCARDED(ch)) {
        // Keep looping if this is a discarded character
        continue;
      }
      break;
    }
    if (bp == endbp) {
      PRInt32 oldLength = bp - mTransformBuf.GetBuffer();
      nsresult rv = mTransformBuf.GrowBy(1000);
      if (NS_FAILED(rv)) {
        // If we run out of space (unlikely) then just chop the input
        break;
      }
      bp = mTransformBuf.GetBuffer() + oldLength;
      endbp = mTransformBuf.GetBufferEnd();
    }
    *bp++ = ' ';
    mBufferPos++;
  }

  *aWordLen = mBufferPos - prevBufferPos;
  return offset;
}

// wordlen==*aWordLen, contentlen=newOffset-currentOffset, isWhitespace=f
PRInt32
nsTextTransformer::ScanPreData_F(PRInt32* aWordLen,
                                 PRBool*  aWasTransformed)
{
  const nsTextFragment* frag = mFrag;
  PRInt32 fragLen = frag->GetLength();
  PRInt32 offset = mOffset;
  PRUnichar* bp = mTransformBuf.GetBuffer() + mBufferPos;
  PRUnichar* endbp = mTransformBuf.GetBufferEnd();
  PRInt32 prevBufferPos = mBufferPos;

  for (; offset < fragLen; offset++) {
    // This function is used for both Unicode and ascii strings so don't
    // make any assumptions about what kind of data it is
    PRUnichar ch = frag->CharAt(offset);
    if ((ch == '\t') || (ch == '\n')) {
      break;
    }
    if (CH_NBSP == ch) {
      ch = ' ';
      *aWasTransformed = PR_TRUE;
    }
    else if (IS_DISCARDED(ch)) {
      continue;
    }
    if (ch > MAX_UNIBYTE) SetHasMultibyte(PR_TRUE);
    if (bp == endbp) {
      PRInt32 oldLength = bp - mTransformBuf.GetBuffer();
      nsresult rv = mTransformBuf.GrowBy(1000);
      if (NS_FAILED(rv)) {
        // If we run out of space (unlikely) then just chop the input
        break;
      }
      bp = mTransformBuf.GetBuffer() + oldLength;
      endbp = mTransformBuf.GetBufferEnd();
    }
    *bp++ = ch;
    mBufferPos++;
  }

  *aWordLen = mBufferPos - prevBufferPos;
  return offset;
}

// wordlen==*aWordLen, contentlen=newOffset-currentOffset, isWhitespace=f
PRInt32
nsTextTransformer::ScanPreAsciiData_F(PRInt32* aWordLen,
                                      PRBool*  aWasTransformed)
{
  const nsTextFragment* frag = mFrag;
  PRUnichar* bp = mTransformBuf.GetBuffer() + mBufferPos;
  PRUnichar* endbp = mTransformBuf.GetBufferEnd();
  const unsigned char* cp = (const unsigned char*) frag->Get1b();
  const unsigned char* end = cp + frag->GetLength();
  PRInt32 prevBufferPos = mBufferPos;
  cp += mOffset;

  while (cp < end) {
    PRUnichar ch = (PRUnichar) *cp++;
    if ((ch == '\t') || (ch == '\n')) {
      cp--;
      break;
    }
    if (CH_NBSP == ch) {
      ch = ' ';
      *aWasTransformed = PR_TRUE;
    }
    else if (IS_DISCARDED(ch)) {
      continue;
    }
    if (ch > MAX_UNIBYTE) SetHasMultibyte(PR_TRUE);
    if (bp == endbp) {
      PRInt32 oldLength = bp - mTransformBuf.GetBuffer();
      nsresult rv = mTransformBuf.GrowBy(1000);
      if (NS_FAILED(rv)) {
        // If we run out of space (unlikely) then just chop the input
        break;
      }
      bp = mTransformBuf.GetBuffer() + oldLength;
      endbp = mTransformBuf.GetBufferEnd();
    }
    *bp++ = ch;
    mBufferPos++;
  }

  *aWordLen = mBufferPos - prevBufferPos;
  return cp - ((const unsigned char*)frag->Get1b());
}

//----------------------------------------

static void
AsciiToLowerCase(unsigned char* aText, PRInt32 aWordLen)
{
  while (aWordLen-- > 0) {
    *aText = tolower(*aText);
    aText++;
  }
}

static void
AsciiToUpperCase(unsigned char* aText, PRInt32 aWordLen)
{
  while (aWordLen-- > 0) {
    *aText = toupper(*aText);
    aText++;
  }
}

#define kSzlig 0x00DF
static PRInt32 CountGermanSzlig(const PRUnichar* aText, PRInt32 len)
{
  PRInt32 i,cnt;
  for(i=0,cnt=0; i<len; i++, aText++)
  {
     if(kSzlig == *aText)
         cnt++;
  }
  return cnt;
}
static void ReplaceGermanSzligToSS(PRUnichar* aText, PRInt32 len, PRInt32 szCnt)
{
  PRUnichar *src, *dest;
  src = aText + len - 1;
  dest = src + szCnt;
  while( (src!=dest) && (src >= aText) )
  {
      if(kSzlig == *src )
      {     
        *dest-- = PRUnichar('S');
        *dest-- = PRUnichar('S');
        src--;
      } else {
        *dest-- = *src--;
      }
  }
}

void
nsTextTransformer::LanguageSpecificTransform(PRUnichar* aText, PRInt32 aLen,
                                             PRBool* aWasTransformed)
{
  if (mLanguageSpecificTransformType ==
      eLanguageSpecificTransformType_Japanese) {
    for (PRInt32 i = 0; i < aLen; i++) {
      if (aText[i] == 0x5C) { // BACKSLASH
        aText[i] = 0xA5; // YEN SIGN
        SetHasMultibyte(PR_TRUE);        
        *aWasTransformed = PR_TRUE;
      }
#if 0
      /*
       * We considered doing this, but since some systems may not have fonts
       * with this OVERLINE glyph, we decided not to do this.
       */
      else if (aText[i] == 0x7E) { // TILDE
        aText[i] = 0x203E; // OVERLINE
        SetHasMultibyte(PR_TRUE);        
        *aWasTransformed = PR_TRUE;
      }
#endif
    }
  }
  /* we once do transformation for Korean, but later decide to remove it */
  /* see bug 88050 for more information */
}

PRUnichar*
nsTextTransformer::GetNextWord(PRBool aInWord,
                               PRInt32* aWordLenResult,
                               PRInt32* aContentLenResult,
                               PRBool* aIsWhiteSpaceResult,
                               PRBool* aWasTransformed,
                               PRBool aResetTransformBuf,
                               PRBool aForLineBreak,
                               PRBool aIsKeyboardSelect)
{
  const nsTextFragment* frag = mFrag;
  PRInt32 fragLen = frag->GetLength();
#ifdef IBMBIDI
  if (*aWordLenResult > 0 && *aWordLenResult < fragLen) {
    fragLen = *aWordLenResult;
  }
#endif
  PRInt32 offset = mOffset;
  PRInt32 wordLen = 0;
  PRBool isWhitespace = PR_FALSE;
  PRUnichar* result = nsnull;
  PRBool prevBufferPos;
  PRBool skippedWhitespace = PR_FALSE;

  // Initialize OUT parameter
  *aWasTransformed = PR_FALSE;

  // See if we should reset the current buffer position back to the
  // beginning of the buffer
  if (aResetTransformBuf) {
    mBufferPos = 0;
    SetTransformedTextIsAscii(LeaveAsAscii());
  }
  prevBufferPos = mBufferPos;

  // Fix word breaking problem w/ PREFORMAT and PREWRAP
  // for word breaking, we should really go to the normal code
  if((! aForLineBreak) && (eNormal != mMode))
     mMode = eNormal;

  while (offset < fragLen) {
    PRUnichar firstChar = frag->CharAt(offset);

    // Eat up any discarded characters before dispatching
    if (IS_DISCARDED(firstChar)) {
      offset++;
      continue;
    }

    switch (mMode) {
      default:
      case eNormal:
        if (XP_IS_SPACE(firstChar)) {
          offset = ScanNormalWhiteSpace_F();

          // if this is just a '\n', and characters before and after it are CJK chars, 
          // we will skip this one.
          if (firstChar == '\n' && 
              offset - mOffset == 1 && 
              mOffset > 0 &&
              offset < fragLen) 
          {
            PRUnichar lastChar = frag->CharAt(mOffset - 1);
            PRUnichar nextChar = frag->CharAt(offset);
            if (IS_CJ_CHAR(lastChar) && IS_CJ_CHAR(nextChar)) {
              skippedWhitespace = PR_TRUE;
              --mBufferPos;
              mOffset = offset;
              continue;            }
          }
          if (firstChar != ' ') {
            *aWasTransformed = PR_TRUE;
          }
          wordLen = 1;
          isWhitespace = PR_TRUE;
        }
        else if (CH_NBSP == firstChar && !aForLineBreak) {
          wordLen = 1;
          isWhitespace = PR_TRUE;
          *aWasTransformed = PR_TRUE;

          // Make sure we have enough room in the transform buffer
          if (mBufferPos >= mTransformBuf.mBufferLen) {
             mTransformBuf.GrowBy(128);
          }

          offset++;
          if (TransformedTextIsAscii()) {
            ((unsigned char*)mTransformBuf.mBuffer)[mBufferPos++] = ' ';
          } else {
            mTransformBuf.mBuffer[mBufferPos++] = PRUnichar(' ');
          }
        }
        else if (frag->Is2b()) {
#ifdef IBMBIDI
          wordLen = *aWordLenResult;
#endif
          offset = ScanNormalUnicodeText_F(aForLineBreak, &wordLen, aWasTransformed);
        }
        else {
          if (!aForLineBreak)
            offset = ScanNormalAsciiText_F_ForWordBreak(&wordLen, 
                                                        aWasTransformed, 
                                                        aIsKeyboardSelect);
          else
            offset = ScanNormalAsciiText_F(&wordLen, aWasTransformed);
        }
        break;

      case ePreformatted:
        if (('\n' == firstChar) || ('\t' == firstChar)) {
          mTransformBuf.mBuffer[mBufferPos++] = firstChar;
          offset++;
          wordLen = 1;
          isWhitespace = PR_TRUE;
        }
        else if (frag->Is2b()) {
          offset = ScanPreData_F(&wordLen, aWasTransformed);
        }
        else {
          offset = ScanPreAsciiData_F(&wordLen, aWasTransformed);
        }
        break;

      case ePreWrap:
        if (XP_IS_SPACE(firstChar)) {
          if (('\n' == firstChar) || ('\t' == firstChar)) {
            mTransformBuf.mBuffer[mBufferPos++] = firstChar;
            offset++;
            wordLen = 1;
          }
          else {
            offset = ScanPreWrapWhiteSpace_F(&wordLen);
          }
          isWhitespace = PR_TRUE;
        }
        else if (frag->Is2b()) {
#ifdef IBMBIDI
          wordLen = *aWordLenResult;
#endif
          offset = ScanNormalUnicodeText_F(aForLineBreak, &wordLen, aWasTransformed);
        }
        else {
          if (!aForLineBreak)
            offset = ScanNormalAsciiText_F_ForWordBreak(&wordLen, aWasTransformed, 
                                                        aIsKeyboardSelect);
          else
            offset = ScanNormalAsciiText_F(&wordLen, aWasTransformed);
        }
        break;
    }

    if (TransformedTextIsAscii()) {
      unsigned char* wordPtr = (unsigned char*)mTransformBuf.mBuffer + prevBufferPos;
      
      if (!isWhitespace) {
        switch (mTextTransform) {
        case NS_STYLE_TEXT_TRANSFORM_CAPITALIZE:
          *wordPtr = toupper(*wordPtr);
          break;
        case NS_STYLE_TEXT_TRANSFORM_LOWERCASE:
          AsciiToLowerCase(wordPtr, wordLen);
          break;
        case NS_STYLE_TEXT_TRANSFORM_UPPERCASE:
          AsciiToUpperCase(wordPtr, wordLen);
          break;
        }
        NS_ASSERTION(mLanguageSpecificTransformType ==
                     eLanguageSpecificTransformType_None,
                     "should not be ASCII for language specific transforms");
      }
      result = (PRUnichar*)wordPtr;

    } else {
      result = &mTransformBuf.mBuffer[prevBufferPos];

      if (!isWhitespace) {
        switch (mTextTransform) {
        case NS_STYLE_TEXT_TRANSFORM_CAPITALIZE:
          if(NS_SUCCEEDED(EnsureCaseConv()))
            gCaseConv->ToTitle(result, result, wordLen, !aInWord);
          // if the first character is szlig
          if(kSzlig == *result)
          {
              if ((prevBufferPos + wordLen + 1) >= mTransformBuf.mBufferLen) {
                mTransformBuf.GrowBy(128);
                result = &mTransformBuf.mBuffer[prevBufferPos];
              }
              PRUnichar* src = result +  wordLen;
              while(src>result) 
              {
                  *(src+1) = *src;
                  src--;
              }
              result[0] = PRUnichar('S');
              result[1] = PRUnichar('S');
              wordLen++;
          }
          break;
        case NS_STYLE_TEXT_TRANSFORM_LOWERCASE:
          if(NS_SUCCEEDED(EnsureCaseConv()))
            gCaseConv->ToLower(result, result, wordLen);
          break;
        case NS_STYLE_TEXT_TRANSFORM_UPPERCASE:
          {
            if(NS_SUCCEEDED(EnsureCaseConv()))
              gCaseConv->ToUpper(result, result, wordLen);

            // first we search for German Szlig
            PRInt32 szligCnt = CountGermanSzlig(result, wordLen);
            if(szligCnt > 0) {
              // Make sure we have enough room in the transform buffer
              if ((prevBufferPos + wordLen + szligCnt) >= mTransformBuf.mBufferLen) 
              {
                mTransformBuf.GrowBy(128);
                result = &mTransformBuf.mBuffer[prevBufferPos];
              }
              ReplaceGermanSzligToSS(result, wordLen, szligCnt);
              wordLen += szligCnt;
            }
          }
          break;
        }
        if (mLanguageSpecificTransformType !=
            eLanguageSpecificTransformType_None) {
          LanguageSpecificTransform(result, wordLen, aWasTransformed);
        }
        if (NeedsArabicShaping()) {
          DoArabicShaping(result, wordLen, aWasTransformed);
        }
        if (NeedsNumericShaping()) {
          DoNumericShaping(result, wordLen, aWasTransformed);
        }
      }
    }

    break;
  }

  *aIsWhiteSpaceResult = isWhitespace;
  *aWordLenResult = wordLen;
  *aContentLenResult = offset - mOffset;

  // we need to adjust the length if a '\n' has been skip between CJK chars
  *aContentLenResult += (skippedWhitespace ? 1 : 0);

  // If the word length doesn't match the content length then we transformed
  // the text
  if ((mTextTransform != NS_STYLE_TEXT_TRANSFORM_NONE) ||
      (*aWordLenResult != *aContentLenResult)) {
    *aWasTransformed = PR_TRUE;
    mBufferPos = prevBufferPos + *aWordLenResult;
  }

  mOffset = offset;

  NS_ASSERTION(mBufferPos == prevBufferPos + *aWordLenResult, "internal error");
  return result;
}

//----------------------------------------------------------------------

// wordlen==1, contentlen=newOffset-currentOffset, isWhitespace=t
PRInt32
nsTextTransformer::ScanNormalWhiteSpace_B()
{
  const nsTextFragment* frag = mFrag;
  PRInt32 offset = mOffset;

  while (--offset >= 0) {
    PRUnichar ch = frag->CharAt(offset);
    if (!XP_IS_SPACE(ch)) {
      // If character is not discardable then stop looping, otherwise
      // let the discarded character collapse with the other spaces.
      if (!IS_DISCARDED(ch)) {
        break;
      }
    }
  }

  mTransformBuf.mBuffer[mTransformBuf.mBufferLen - 1] = ' ';
  return offset;
}

// wordlen==*aWordLen, contentlen=newOffset-currentOffset, isWhitespace=f
PRInt32
nsTextTransformer::ScanNormalAsciiText_B(PRInt32* aWordLen, PRBool aIsKeyboardSelect)
{
  const nsTextFragment* frag = mFrag;
  PRInt32 offset = mOffset;
  PRUnichar* bp = mTransformBuf.GetBufferEnd();
  PRUnichar* startbp = mTransformBuf.GetBuffer();

  PRUnichar ch = frag->CharAt(offset - 1);
  // Treat high bit chars as alphanumeric, otherwise we get stuck on accented letters
  // We can't trust isalnum() results for isalnum()
  // Therefore we don't stop at non-ascii (high bit) punctuation,
  // which is just fine. The punctuation we care about is low bit.
  PRBool readingAlphaNumeric = isalnum(ch) || !IS_ASCII_CHAR(ch);

  while (--offset >= 0) {
    PRUnichar ch = frag->CharAt(offset);
    if (CH_NBSP == ch) {
      ch = ' ';
    }
    if (XP_IS_SPACE(ch)) {
      break;
    }
    else if (IS_DISCARDED(ch)) {
      continue;
    } 
    else if (sWordSelectStopAtPunctuation && readingAlphaNumeric && 
             !isalnum(ch) && IS_ASCII_CHAR(ch)) {
      // Break on ascii punctuation
      break;
    }
    else if (sWordSelectStopAtPunctuation && !readingAlphaNumeric &&
             (isalnum(ch) || !IS_ASCII_CHAR(ch))) {
      if (!aIsKeyboardSelect)
        break;
      readingAlphaNumeric = PR_TRUE;
    }
    
    if (ch > MAX_UNIBYTE) SetHasMultibyte(PR_TRUE);
    if (bp == startbp) {
      PRInt32 oldLength = mTransformBuf.mBufferLen;
      nsresult rv = mTransformBuf.GrowBy(1000);
      if (NS_FAILED(rv)) {
        // If we run out of space (unlikely) then just chop the input
        break;
      }
      bp = mTransformBuf.GetBufferEnd() - oldLength;
      startbp = mTransformBuf.GetBuffer();
    }
    *--bp = ch;
  }

  *aWordLen = mTransformBuf.GetBufferEnd() - bp;
  return offset;
}

// wordlen==*aWordLen, contentlen=newOffset-currentOffset, isWhitespace=f
PRInt32
nsTextTransformer::ScanNormalUnicodeText_B(PRBool aForLineBreak,
                                           PRInt32* aWordLen)
{
  const nsTextFragment* frag = mFrag;
  const PRUnichar* cp0 = frag->Get2b();
  PRInt32 offset = mOffset - 1;

  PRUnichar firstChar = frag->CharAt(offset);

#ifdef IBMBIDI
  PRInt32 limit = (*aWordLen > 0) ? *aWordLen : 0;
  
  while (offset > limit && IS_BIDI_CONTROL(firstChar) ) {
    firstChar = frag->CharAt(--offset);
  }
#endif

  mTransformBuf.mBuffer[mTransformBuf.mBufferLen - 1] = firstChar;
  if (firstChar > MAX_UNIBYTE) SetHasMultibyte(PR_TRUE);

  PRInt32 numChars = 1;

#ifdef IBMBIDI
  if (offset > limit) {
#else
  if (offset > 0) {
#endif
    const PRUnichar* cp = cp0 + offset;
    PRBool breakBetween = PR_FALSE;
    if (aForLineBreak) {
      mLineBreaker->BreakInBetween(cp0, offset + 1,
                                   mTransformBuf.GetBufferEnd()-1, 1,
                                   &breakBetween);
    }
    else {
      mWordBreaker->BreakInBetween(cp0, offset + 1,
                                   mTransformBuf.GetBufferEnd()-1, 1,
                                   &breakBetween);
    }

    if (!breakBetween) {
      // Find next position
      PRBool tryPrevFrag;
      PRUint32 prev;
      if (aForLineBreak) {
        mLineBreaker->Prev(cp0, offset, offset, &prev, &tryPrevFrag);
      }
      else {
        mWordBreaker->PrevWord(cp0, offset, offset, &prev, &tryPrevFrag);
      }
      numChars = (PRInt32) ((PRUint32) offset - prev) + 1;

      // Grow buffer before copying
      nsresult rv = mTransformBuf.GrowTo(numChars);
      if (NS_FAILED(rv)) {
        numChars = mTransformBuf.GetBufferLength();
      }

      // 1. convert nbsp into space
      // 2. check mHasMultibyte flag
      // 3. copy buffer
      PRUnichar* bp = mTransformBuf.GetBufferEnd() - 1;
      const PRUnichar* end = cp - numChars + 1;
      while (cp > end) {
        PRUnichar ch = *--cp;
        if (CH_NBSP == ch) {
          ch = ' ';
        }
        else if (IS_DISCARDED(ch)) {
          continue;
        }
        if (ch > MAX_UNIBYTE) SetHasMultibyte(PR_TRUE);
        *--bp = ch;
      }

      // Recompute offset and numChars in case we stripped something
      offset = offset - numChars;
      numChars =  mTransformBuf.GetBufferEnd() - bp;
    }
  }
  else 
	  offset--;

  *aWordLen = numChars;
  return offset;
}

// wordlen==*aWordLen, contentlen=newOffset-currentOffset, isWhitespace=t
PRInt32
nsTextTransformer::ScanPreWrapWhiteSpace_B(PRInt32* aWordLen)
{
  const nsTextFragment* frag = mFrag;
  PRInt32 offset = mOffset;
  PRUnichar* bp = mTransformBuf.GetBufferEnd();
  PRUnichar* startbp = mTransformBuf.GetBuffer();

  while (--offset >= 0) {
    PRUnichar ch = frag->CharAt(offset);
    if (!XP_IS_SPACE(ch) || (ch == '\t') || (ch == '\n')) {
      // Keep looping if this is a discarded character
      if (IS_DISCARDED(ch)) {
        continue;
      }
      break;
    }
    if (bp == startbp) {
      PRInt32 oldLength = mTransformBuf.mBufferLen;
      nsresult rv = mTransformBuf.GrowBy(1000);
      if (NS_FAILED(rv)) {
        // If we run out of space (unlikely) then just chop the input
        break;
      }
      bp = mTransformBuf.GetBufferEnd() - oldLength;
      startbp = mTransformBuf.GetBuffer();
    }
    *--bp = ' ';
  }

  *aWordLen = mTransformBuf.GetBufferEnd() - bp;
  return offset;
}

// wordlen==*aWordLen, contentlen=newOffset-currentOffset, isWhitespace=f
PRInt32
nsTextTransformer::ScanPreData_B(PRInt32* aWordLen)
{
  const nsTextFragment* frag = mFrag;
  PRInt32 offset = mOffset;
  PRUnichar* bp = mTransformBuf.GetBufferEnd();
  PRUnichar* startbp = mTransformBuf.GetBuffer();

  while (--offset >= 0) {
    PRUnichar ch = frag->CharAt(offset);
    if ((ch == '\t') || (ch == '\n')) {
      break;
    }
    if (CH_NBSP == ch) {
      ch = ' ';
    }
    else if (IS_DISCARDED(ch)) {
      continue;
    }
    if (ch > MAX_UNIBYTE) SetHasMultibyte(PR_TRUE);
    if (bp == startbp) {
      PRInt32 oldLength = mTransformBuf.mBufferLen;
      nsresult rv = mTransformBuf.GrowBy(1000);
      if (NS_FAILED(rv)) {
        // If we run out of space (unlikely) then just chop the input
        offset++;
        break;
      }
      bp = mTransformBuf.GetBufferEnd() - oldLength;
      startbp = mTransformBuf.GetBuffer();
    }
    *--bp = ch;
  }

  *aWordLen = mTransformBuf.GetBufferEnd() - bp;
  return offset;
}

//----------------------------------------

PRUnichar*
nsTextTransformer::GetPrevWord(PRBool aInWord,
                               PRInt32* aWordLenResult,
                               PRInt32* aContentLenResult,
                               PRBool* aIsWhiteSpaceResult,
                               PRBool aForLineBreak,
                               PRBool aIsKeyboardSelect)
{
  const nsTextFragment* frag = mFrag;
  PRInt32 offset = mOffset;
  PRInt32 wordLen = 0;
  PRBool isWhitespace = PR_FALSE;
  PRUnichar* result = nsnull;

  // Fix word breaking problem w/ PREFORMAT and PREWRAP
  // for word breaking, we should really go to the normal code
  if((! aForLineBreak) && (eNormal != mMode))
     mMode = eNormal;

#ifdef IBMBIDI
  PRInt32 limit = (*aWordLenResult > 0) ? *aWordLenResult : 0;
  while (--offset >= limit) {
#else
  while (--offset >= 0) {
#endif
    PRUnichar firstChar = frag->CharAt(offset);

    // Eat up any discarded characters before dispatching
    if (IS_DISCARDED(firstChar)) {
      continue;
    }

    switch (mMode) {
      default:
      case eNormal:
        if (XP_IS_SPACE(firstChar)) {
          offset = ScanNormalWhiteSpace_B();
          wordLen = 1;
          isWhitespace = PR_TRUE;
        }
        else if (CH_NBSP == firstChar && !aForLineBreak) {
          wordLen = 1;
          isWhitespace = PR_TRUE;
          mTransformBuf.mBuffer[mTransformBuf.mBufferLen - 1] = ' ';
          offset--;
        } else if (frag->Is2b()) {
#ifdef IBMBIDI
          wordLen = *aWordLenResult;
#endif
          offset = ScanNormalUnicodeText_B(aForLineBreak, &wordLen);
        }
        else {
          offset = ScanNormalAsciiText_B(&wordLen, aIsKeyboardSelect);
        }
        break;

      case ePreformatted:
        if (('\n' == firstChar) || ('\t' == firstChar)) {
          mTransformBuf.mBuffer[mTransformBuf.mBufferLen-1] = firstChar;
          offset--;  // make sure we overshoot
          wordLen = 1;
          isWhitespace = PR_TRUE;
        }
        else {
          offset = ScanPreData_B(&wordLen);
        }
        break;

      case ePreWrap:
        if (XP_IS_SPACE(firstChar)) {
          if (('\n' == firstChar) || ('\t' == firstChar)) {
            mTransformBuf.mBuffer[mTransformBuf.mBufferLen-1] = firstChar;
            offset--;  // make sure we overshoot
            wordLen = 1;
          }
          else {
            offset = ScanPreWrapWhiteSpace_B(&wordLen);
          }
          isWhitespace = PR_TRUE;
        }
        else if (frag->Is2b()) {
#ifdef IBMBIDI
          wordLen = *aWordLenResult;
#endif
          offset = ScanNormalUnicodeText_B(aForLineBreak, &wordLen);
        }
        else {
          offset = ScanNormalAsciiText_B(&wordLen, aIsKeyboardSelect);
        }
        break;
    }

    // Backwards scanning routines *always* overshoot by one for the
    // returned offset value.
    offset = offset + 1;

    result = mTransformBuf.GetBufferEnd() - wordLen;

    if (!isWhitespace) {
      switch (mTextTransform) {
        case NS_STYLE_TEXT_TRANSFORM_CAPITALIZE:
          if(NS_SUCCEEDED(EnsureCaseConv()))
            gCaseConv->ToTitle(result, result, wordLen, !aInWord);
          break;
        case NS_STYLE_TEXT_TRANSFORM_LOWERCASE:
          if(NS_SUCCEEDED(EnsureCaseConv()))
            gCaseConv->ToLower(result, result, wordLen);
          break;
        case NS_STYLE_TEXT_TRANSFORM_UPPERCASE:
          if(NS_SUCCEEDED(EnsureCaseConv()))
            gCaseConv->ToUpper(result, result, wordLen);
          break;
      }
    }
    break;
  }

  *aWordLenResult = wordLen;
  *aContentLenResult = mOffset - offset;
  *aIsWhiteSpaceResult = isWhitespace;

  mOffset = offset;
  return result;
}

void
nsTextTransformer::DoArabicShaping(PRUnichar* aText, 
                                   PRInt32& aTextLength, 
                                   PRBool* aWasTransformed)
{
  if (aTextLength <= 0)
    return;
  
  PRInt32 newLen;
  PRBool isVisual = mPresContext->IsVisualMode();

  nsAutoString buf;
  buf.SetLength(aTextLength);
  PRUnichar* buffer = buf.BeginWriting();
  
  ArabicShaping(aText, buf.Length(), buffer, (PRUint32 *)&newLen, !isVisual, !isVisual);

  aTextLength = newLen;
  *aWasTransformed = PR_TRUE;

  StripZeroWidthJoinControls(buffer, aText, aTextLength, aWasTransformed);
}

void
nsTextTransformer::DoNumericShaping(PRUnichar* aText, 
                                    PRInt32& aTextLength, 
                                    PRBool* aWasTransformed)
{
  if (aTextLength <= 0)
    return;

  PRUint32 bidiOptions = mPresContext->GetBidi();

  switch (GET_BIDI_OPTION_NUMERAL(bidiOptions)) {

    case IBMBIDI_NUMERAL_HINDI:
      HandleNumbers(aText, aTextLength, IBMBIDI_NUMERAL_HINDI);
      break;

    case IBMBIDI_NUMERAL_ARABIC:
      HandleNumbers(aText, aTextLength, IBMBIDI_NUMERAL_ARABIC);
      break;

    case IBMBIDI_NUMERAL_REGULAR:

      switch (mCharType) {

        case eCharType_EuropeanNumber:
          HandleNumbers(aText, aTextLength, IBMBIDI_NUMERAL_ARABIC);
          break;

        case eCharType_ArabicNumber:
          HandleNumbers(aText, aTextLength, IBMBIDI_NUMERAL_HINDI);
          break;

        default:
          break;
      }
      break;

    case IBMBIDI_NUMERAL_HINDICONTEXT:
      if (((GET_BIDI_OPTION_DIRECTION(bidiOptions)==IBMBIDI_TEXTDIRECTION_RTL) &&
           (IS_ARABIC_DIGIT (aText[0]))) ||
          (eCharType_ArabicNumber == mCharType))
        HandleNumbers(aText, aTextLength, IBMBIDI_NUMERAL_HINDI);
      else if (eCharType_EuropeanNumber == mCharType)
        HandleNumbers(aText, aTextLength, IBMBIDI_NUMERAL_ARABIC);
      break;

    case IBMBIDI_NUMERAL_NOMINAL:
    default:
      break;
  }
}

void
nsTextTransformer::StripZeroWidthJoinControls(PRUnichar* aSource,
                                              PRUnichar* aTarget,
                                              PRInt32& aTextLength, 
                                              PRBool* aWasTransformed)
{
  PRUnichar *src, *dest;
  PRInt32 stripped = 0;

  src = aSource;
  dest = aTarget;

  for (PRInt32 i = 0; i < aTextLength; ++i) {
    while (*src == CH_ZWNJ || *src == CH_ZWJ) {
      ++stripped;
      ++src;
      *aWasTransformed = PR_TRUE;
    }
    *dest++ = *src++;
  }
  aTextLength -= stripped;
}

//----------------------------------------------------------------------
// Self test logic for this class. This will (hopefully) make sure
// that the forward and backward word iterator methods continue to
// function as people change things...

#ifdef DEBUG
struct SelfTestSection {
  int length;
  int* data;
};

#define NUM_MODES 3

struct SelfTestData {
  const PRUnichar* text;
  SelfTestSection modes[NUM_MODES];
};

static PRUint8 preModeValue[NUM_MODES] = {
  NS_STYLE_WHITESPACE_NORMAL,
  NS_STYLE_WHITESPACE_PRE,
  NS_STYLE_WHITESPACE_MOZ_PRE_WRAP
};

static PRUnichar test1text[] = {
  'o', 'n', 'c', 'e', ' ', 'u', 'p', 'o', 'n', '\t',
  'a', ' ', 's', 'h', 'o', 'r', 't', ' ', 't', 'i', 'm', 'e', 0
};
static int test1Results[] = { 4, 1, 4, 1, 1, 1, 5, 1, 4 };
static int test1PreResults[] = { 9, 1, 12 };
static int test1PreWrapResults[] = { 4, 1, 4, 1, 1, 1, 5, 1, 4 };

static PRUnichar test2text[] = {
  0xF6, 'n', 'c', 'e', ' ', 0xFB, 'p', 'o', 'n', '\t',
  0xE3, ' ', 's', 'h', 0xF3, 'r', 't', ' ', 't', 0xEE, 'm', 'e', ' ', 0
};
static int test2Results[] = { 4, 1, 4, 1, 1, 1, 5, 1, 4, 1 };
static int test2PreResults[] = { 9, 1, 13 };
static int test2PreWrapResults[] = { 4, 1, 4, 1, 1, 1, 5, 1, 4, 1 };

static PRUnichar test3text[] = {
  0x0152, 'n', 'c', 'e', ' ', 'x', 'y', '\t', 'z', 'y', ' ', 0
};
static int test3Results[] = { 4, 1, 2, 1, 2, 1, };
static int test3PreResults[] = { 7, 1, 3, };
static int test3PreWrapResults[] = { 4, 1, 2, 1, 2, 1, };

static PRUnichar test4text[] = {
  'o', 'n', CH_SHY, 'c', 'e', ' ', CH_SHY, ' ', 'u', 'p', 'o', 'n', '\t',
  'a', ' ', 's', 'h', 'o', 'r', 't', ' ', 't', 'i', 'm', 'e', 0
};
static int test4Results[] = { 4, 1, 4, 1, 1, 1, 5, 1, 4 };
static int test4PreResults[] = { 10, 1, 12 };
static int test4PreWrapResults[] = { 4, 2, 4, 1, 1, 1, 5, 1, 4 };

static PRUnichar test5text[] = {
  CH_SHY, 0
};
static int test5Results[] = { 0 };
static int test5PreResults[] = { 0 };
static int test5PreWrapResults[] = { 0 };

#if 0
static PRUnichar test6text[] = {
  0x30d5, 0x30b8, 0x30c6, 0x30ec, 0x30d3, 0x306e, 0x97f3, 0x697d,
  0x756a, 0x7d44, 0x300c, 'H', 'E', 'Y', '!', ' ', 'H', 'E', 'Y', '!',
  '\t', 'H', 'E', 'Y', '!', 0x300d, 0x306e, 0x30db, 0x30fc, 0x30e0,
  0x30da, 0x30fc, 0x30b8, 0x3002, 0
};
static int test6Results[] = { 1, 1, 1, 1, 1,
                              1, 1, 1, 1, 1,
                              5, 1, 4, 1, 5,
                              1, 2, 1, 2, 2 };
static int test6PreResults[] = { 20, 1, 13 };
static int test6PreWrapResults[] = { 1, 1, 1, 1, 1,
                                     1, 1, 1, 1, 1,
                                     5, 1, 4, 1, 5,
                                     1, 2, 1, 2, 2 };
#endif

static SelfTestData tests[] = {
  { test1text,
    { { sizeof(test1Results)/sizeof(int), test1Results, },
      { sizeof(test1PreResults)/sizeof(int), test1PreResults, },
      { sizeof(test1PreWrapResults)/sizeof(int), test1PreWrapResults, } }
  },
  { test2text,
    { { sizeof(test2Results)/sizeof(int), test2Results, },
      { sizeof(test2PreResults)/sizeof(int), test2PreResults, },
      { sizeof(test2PreWrapResults)/sizeof(int), test2PreWrapResults, } }
  },
  { test3text,
    { { sizeof(test3Results)/sizeof(int), test3Results, },
      { sizeof(test3PreResults)/sizeof(int), test3PreResults, },
      { sizeof(test3PreWrapResults)/sizeof(int), test3PreWrapResults, } }
  },
  { test4text,
    { { sizeof(test4Results)/sizeof(int), test4Results, },
      { sizeof(test4PreResults)/sizeof(int), test4PreResults, },
      { sizeof(test4PreWrapResults)/sizeof(int), test4PreWrapResults, } }
  },
  { test5text,
    { { sizeof(test5Results)/sizeof(int), test5Results, },
      { sizeof(test5PreResults)/sizeof(int), test5PreResults, },
      { sizeof(test5PreWrapResults)/sizeof(int), test5PreWrapResults, } }
  },
#if 0
  { test6text,
    { { sizeof(test6Results)/sizeof(int), test6Results, },
      { sizeof(test6PreResults)/sizeof(int), test6PreResults, },
      { sizeof(test6PreWrapResults)/sizeof(int), test6PreWrapResults, } }
  },
#endif
};

#define NUM_TESTS (sizeof(tests) / sizeof(tests[0]))

void
nsTextTransformer::SelfTest(nsILineBreaker* aLineBreaker,
                            nsIWordBreaker* aWordBreaker,
                            nsPresContext* aPresContext)
{
  PRBool gNoisy = PR_FALSE;
  if (PR_GetEnv("GECKO_TEXT_TRANSFORMER_NOISY_SELF_TEST")) {
    gNoisy = PR_TRUE;
  }

  PRBool error = PR_FALSE;
  PRInt32 testNum = 0;
  SelfTestData* st = tests;
  SelfTestData* last = st + NUM_TESTS;
  for (; st < last; st++) {
    PRUnichar* bp;
    PRInt32 wordLen, contentLen;
    PRBool ws, transformed;

    PRBool isAsciiTest = PR_TRUE;
    const PRUnichar* cp = st->text;
    while (*cp) {
      if (*cp > 255) {
        isAsciiTest = PR_FALSE;
        break;
      }
      cp++;
    }

    nsTextFragment frag(st->text);
    nsTextTransformer tx(aLineBreaker, aWordBreaker, aPresContext);

    for (PRInt32 preMode = 0; preMode < NUM_MODES; preMode++) {
      // Do forwards test
      if (gNoisy) {
        nsAutoString uc2(st->text);
        printf("%s forwards test: '", isAsciiTest ? "ascii" : "unicode");
        fputs(NS_ConvertUCS2toUTF8(uc2).get(), stdout);
        printf("'\n");
      }
      tx.Init2(&frag, 0, preModeValue[preMode], NS_STYLE_TEXT_TRANSFORM_NONE);

      int* expectedResults = st->modes[preMode].data;
      int resultsLen = st->modes[preMode].length;

#ifdef IBMBIDI
      wordLen = -1;
#endif
      while ((bp = tx.GetNextWord(PR_FALSE, &wordLen, &contentLen, &ws, &transformed))) {
        if (gNoisy) {
          nsAutoString tmp(bp, wordLen);
          printf("  '");
          fputs(NS_ConvertUCS2toUTF8(tmp).get(), stdout);
          printf("': ws=%s wordLen=%d (%d) contentLen=%d (offset=%d)\n",
                 ws ? "yes" : "no",
                 wordLen, *expectedResults, contentLen, tx.mOffset);
        }
        if (*expectedResults != wordLen) {
          error = PR_TRUE;
          break;
        }
        expectedResults++;
#ifdef IBMBIDI
        wordLen = -1;
#endif
      }
      if (expectedResults != st->modes[preMode].data + resultsLen) {
        if (st->modes[preMode].data[0] != 0) {
          error = PR_TRUE;
        }
      }

      // Do backwards test
      if (gNoisy) {
        nsAutoString uc2(st->text);
        printf("%s backwards test: '", isAsciiTest ? "ascii" : "unicode");
        fputs(NS_ConvertUCS2toUTF8(uc2).get(), stdout);
        printf("'\n");
      }
      tx.Init2(&frag, frag.GetLength(), NS_STYLE_WHITESPACE_NORMAL,
               NS_STYLE_TEXT_TRANSFORM_NONE);
      expectedResults = st->modes[preMode].data + resultsLen;
#ifdef IBMBIDI
      wordLen = -1;
#endif
      while ((bp = tx.GetPrevWord(PR_FALSE, &wordLen, &contentLen, &ws))) {
        --expectedResults;
        if (gNoisy) {
          nsAutoString tmp(bp, wordLen);
          printf("  '");
          fputs(NS_ConvertUCS2toUTF8(tmp).get(), stdout);
          printf("': ws=%s wordLen=%d contentLen=%d (offset=%d)\n",
                 ws ? "yes" : "no",
                 wordLen, contentLen, tx.mOffset);
        }
        if (*expectedResults != wordLen) {
          error = PR_TRUE;
          break;
        }
#ifdef IBMBIDI
        wordLen = -1;
#endif
      }
      if (expectedResults != st->modes[preMode].data) {
        if (st->modes[preMode].data[0] != 0) {
          error = PR_TRUE;
        }
      }

      if (error) {
        fprintf(stderr, "nsTextTransformer: self test %d failed\n", testNum);
      }
      else if (gNoisy) {
        fprintf(stdout, "nsTextTransformer: self test %d succeeded\n", testNum);
      }

      testNum++;
    }
  }
  if (error) {
    NS_ABORT();
  }
}

nsresult
nsTextTransformer::Init2(const nsTextFragment* aFrag,
                         PRInt32 aStartingOffset,
                         PRUint8 aWhiteSpace,
                         PRUint8 aTextTransform)
{
  mFrag = aFrag;

  // Sanitize aStartingOffset
  if (aStartingOffset < 0) {
    NS_WARNING("bad starting offset");
    aStartingOffset = 0;
  }
  else if (aStartingOffset > mFrag->GetLength()) {
    NS_WARNING("bad starting offset");
    aStartingOffset = mFrag->GetLength();
  }
  mOffset = aStartingOffset;

  // Get the frames text style information
  if (NS_STYLE_WHITESPACE_PRE == aWhiteSpace) {
    mMode = ePreformatted;
  }
  else if (NS_STYLE_WHITESPACE_MOZ_PRE_WRAP == aWhiteSpace) {
    mMode = ePreWrap;
  }
  mTextTransform = aTextTransform;

  return NS_OK;
}
#endif /* DEBUG */
