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
#include <ctype.h>
#include "nsCOMPtr.h"
#include "nsTextTransformer.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIStyleContext.h"
#include "nsITextContent.h"
#include "nsStyleConsts.h"
#include "nsILineBreaker.h"
#include "nsIWordBreaker.h"
#include "nsHTMLIIDs.h"
#include "nsIServiceManager.h"
#include "nsUnicharUtilCIID.h"
#include "nsICaseConversion.h"
#include "prenv.h"
#include "nsIPref.h"


PRPackedBool nsTextTransformer::sWordSelectPrefInited = PR_FALSE;
PRPackedBool nsTextTransformer::sWordSelectStopAtPunctuation = PR_FALSE;


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
    nsCRT::memcpy(&newBuffer[aCopyToHead ? 0 : mBufferLen],
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
  nsresult res = NS_OK;
  
  // read in our global word selection prefs
  if ( !sWordSelectPrefInited ) {
    nsCOMPtr<nsIPref> prefService ( do_GetService(NS_PREF_CONTRACTID) );
    if ( prefService ) {
      PRBool temp = PR_FALSE;
      prefService->GetBoolPref("layout.word_select.stop_at_punctuation", &temp);
      sWordSelectStopAtPunctuation = temp;
    }
    sWordSelectPrefInited = PR_TRUE;
  }
  
  return res;
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
#define IS_DISCARDED(_ch) \
  (((_ch) == CH_SHY) || ((_ch) == '\r'))

#define MAX_UNIBYTE 127

MOZ_DECL_CTOR_COUNTER(nsTextTransformer)

nsTextTransformer::nsTextTransformer(nsILineBreaker* aLineBreaker,
                                     nsIWordBreaker* aWordBreaker,
                                     nsIPresContext* aPresContext)
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

  aPresContext->
    GetLanguageSpecificTransformType(&mLanguageSpecificTransformType);

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
                        PRBool aLeaveAsAscii)
{
  // Get the contents text content
  nsresult rv;
  nsCOMPtr<nsITextContent> tc = do_QueryInterface(aContent, &rv);
  if (tc.get()) {
    tc->GetText(&mFrag);

    // Sanitize aStartingOffset
    if (NS_WARN_IF_FALSE(aStartingOffset >= 0, "bad starting offset")) {
      aStartingOffset = 0;
    }
    else if (NS_WARN_IF_FALSE(aStartingOffset <= mFrag->GetLength(),
                              "bad starting offset")) {
      aStartingOffset = mFrag->GetLength();
    }
    mOffset = aStartingOffset;

    // Get the frames text style information
    const nsStyleText* styleText;
    aFrame->GetStyleData(eStyleStruct_Text, (const nsStyleStruct*&) styleText);
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
                                         PRBool*  aWasTransformed)
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
    else if (sWordSelectStopAtPunctuation && !isalnum(ch)) {
      // on some platforms, punctuation breaks words too.
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
  PRInt32 offset = mOffset;

  PRUnichar firstChar = frag->CharAt(offset++);
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
        mWordBreaker->Next(cp0, fragLen, offset, &next, &tryNextFrag);
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
                               PRBool aForLineBreak)
{
  const nsTextFragment* frag = mFrag;
  PRInt32 fragLen = frag->GetLength();
  PRInt32 offset = mOffset;
  PRInt32 wordLen = 0;
  PRBool isWhitespace = PR_FALSE;
  PRUnichar* result = nsnull;
  PRBool prevBufferPos;

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
          offset = ScanNormalUnicodeText_F(aForLineBreak, &wordLen, aWasTransformed);
        }
        else {
          if (!aForLineBreak)
            offset = ScanNormalAsciiText_F_ForWordBreak(&wordLen, aWasTransformed);
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
          offset = ScanNormalUnicodeText_F(aForLineBreak, &wordLen, aWasTransformed);
        }
        else {
          if (!aForLineBreak)
            offset = ScanNormalAsciiText_F_ForWordBreak(&wordLen, aWasTransformed);
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
      }
    }

    break;
  }

  *aWordLenResult = wordLen;
  *aContentLenResult = offset - mOffset;
  *aIsWhiteSpaceResult = isWhitespace;

  // If the word length doesn't match the content length then we transformed
  // the text
  if ((mTextTransform != NS_STYLE_TEXT_TRANSFORM_NONE) ||
      (*aWordLenResult != *aContentLenResult)) {
    *aWasTransformed = PR_TRUE;
  }

  mOffset = offset;
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
nsTextTransformer::ScanNormalAsciiText_B(PRInt32* aWordLen)
{
  const nsTextFragment* frag = mFrag;
  PRInt32 offset = mOffset;
  PRUnichar* bp = mTransformBuf.GetBufferEnd();
  PRUnichar* startbp = mTransformBuf.GetBuffer();

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
  mTransformBuf.mBuffer[mTransformBuf.mBufferLen - 1] = firstChar;
  if (firstChar > MAX_UNIBYTE) SetHasMultibyte(PR_TRUE);

  PRInt32 numChars = 1;
  if (offset > 0) {
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
        mWordBreaker->Prev(cp0, offset, offset, &prev, &tryPrevFrag);
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
                               PRBool aForLineBreak)
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

  while (--offset >= 0) {
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
          offset = ScanNormalUnicodeText_B(aForLineBreak, &wordLen);
        }
        else {
          offset = ScanNormalAsciiText_B(&wordLen);
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
          offset = ScanNormalUnicodeText_B(aForLineBreak, &wordLen);
        }
        else {
          offset = ScanNormalAsciiText_B(&wordLen);
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
                            nsIPresContext* aPresContext)
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
        fputs(NS_LossyConvertUCS2toASCII(uc2).get(), stdout);
        printf("'\n");
      }
      tx.Init2(&frag, 0, preModeValue[preMode], NS_STYLE_TEXT_TRANSFORM_NONE);

      int* expectedResults = st->modes[preMode].data;
      int resultsLen = st->modes[preMode].length;

      while ((bp = tx.GetNextWord(PR_FALSE, &wordLen, &contentLen, &ws, &transformed))) {
        if (gNoisy) {
          nsAutoString tmp(bp, wordLen);
          printf("  '");
          fputs(NS_LossyConvertUCS2toASCII(tmp).get(), stdout);
          printf("': ws=%s wordLen=%d (%d) contentLen=%d (offset=%d)\n",
                 ws ? "yes" : "no",
                 wordLen, *expectedResults, contentLen, tx.mOffset);
        }
        if (*expectedResults != wordLen) {
          error = PR_TRUE;
          break;
        }
        expectedResults++;
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
        fputs(NS_LossyConvertUCS2toASCII(uc2).get(), stdout);
        printf("'\n");
      }
      tx.Init2(&frag, frag.GetLength(), NS_STYLE_WHITESPACE_NORMAL,
               NS_STYLE_TEXT_TRANSFORM_NONE);
      expectedResults = st->modes[preMode].data + resultsLen;
      while ((bp = tx.GetPrevWord(PR_FALSE, &wordLen, &contentLen, &ws))) {
        --expectedResults;
        if (gNoisy) {
          nsAutoString tmp(bp, wordLen);
          printf("  '");
          fputs(NS_LossyConvertUCS2toASCII(tmp).get(), stdout);
          printf("': ws=%s wordLen=%d contentLen=%d (offset=%d)\n",
                 ws ? "yes" : "no",
                 wordLen, contentLen, tx.mOffset);
        }
        if (*expectedResults != wordLen) {
          error = PR_TRUE;
          break;
        }
      }
      if (expectedResults != st->modes[preMode].data) {
        if (st->modes[preMode].data[0] != 0) {
          error = PR_TRUE;
        }
      }

      if (error) {
        fprintf(stderr, "nsTextTransformer: self test %d failed\n", testNum);
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
  if (NS_WARN_IF_FALSE(aStartingOffset >= 0, "bad starting offset")) {
    aStartingOffset = 0;
  }
  else if (NS_WARN_IF_FALSE(aStartingOffset <= mFrag->GetLength(),
                            "bad starting offset")) {
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
