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
#include "nsTextTransformer.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIStyleContext.h"
#include "nsITextContent.h"
#include "nsStyleConsts.h"

// XXX put a copy in nsHTMLIIDs
static NS_DEFINE_IID(kITextContentIID, NS_ITEXT_CONTENT_IID);

// XXX temporary
#define XP_IS_SPACE(_ch) \
  (((_ch) == ' ') || ((_ch) == '\t') || ((_ch) == '\n'))

// XXX need more of this in nsIRenderingContext.GetWidth
#define CH_NBSP 160

#define MAX_UNIBYTE 127

nsTextTransformer::nsTextTransformer(PRUnichar* aBuffer, PRInt32 aBufLen)
  : mAutoBuffer(aBuffer),
    mBuffer(aBuffer),
    mBufferLength(aBufLen < 0 ? 0 : aBufLen),
    mHasMultibyte(PR_FALSE)
{
}

nsTextTransformer::~nsTextTransformer()
{
  if (mBuffer != mAutoBuffer) {
    delete [] mBuffer;
  }
}

nsresult
nsTextTransformer::Init(/*nsTextRun& aTextRun, XXX*/
                        nsIFrame* aFrame,
                        PRInt32 aStartingOffset)
{
  // Make sure we have *some* space in case arguments to the ctor were
  // bizzare.
  if (mBufferLength < 100) {
    if (!GrowBuffer()) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  // Get the frames text content
  nsIContent* content;
  aFrame->GetContent(content);
  nsITextContent* tc;
  if (NS_OK != content->QueryInterface(kITextContentIID, (void**) &tc)) {
    NS_RELEASE(content);
    return NS_OK;
  }
  tc->GetText(mFrags, mNumFrags);
  NS_RELEASE(tc);
  NS_RELEASE(content);
  mStartingOffset = aStartingOffset;
  mOffset = mStartingOffset;

  // Compute the total length of the text content.
  PRInt32 sum = 0;
  PRInt32 n = mNumFrags;
  const nsTextFragment* frag = mFrags;
  for (; --n >= 0; frag++) {
    sum += frag->GetLength();
  }
  mContentLength = sum;

  // Set current fragment and current fragment offset
  mCurrentFrag = mFrags;
  mCurrentFragOffset = 0;
  PRInt32 offset = 0;
  n = mNumFrags;
  for (frag = mFrags; --n >= 0; frag++) {
    if (aStartingOffset < offset + frag->GetLength()) {
      mCurrentFrag = frag;
      mCurrentFragOffset = aStartingOffset - offset;
      break;
    }
    offset += frag->GetLength();
  }

  // Get the frames style and choose a transform proc
  const nsStyleText* styleText;
  aFrame->GetStyleData(eStyleStruct_Text, (const nsStyleStruct*&) styleText);
  mCompressWS = NS_STYLE_WHITESPACE_PRE != styleText->mWhiteSpace;

  return NS_OK;
}

PRBool
nsTextTransformer::GrowBuffer()
{
  PRInt32 newLen = mBufferLength * 2;
  if (newLen <= 100) {
    newLen = 100;
  }
  PRUnichar* newBuffer = new PRUnichar[newLen];
  if (nsnull == newBuffer) {
    return PR_FALSE;
  }
  if (0 != mBufferLength) {
    nsCRT::memcpy(newBuffer, mBuffer, sizeof(PRUnichar) * mBufferLength);
    if (mBuffer != mAutoBuffer) {
      delete [] mBuffer;
    }
  }
  mBuffer = newBuffer;
  return PR_TRUE;
}

PRUnichar*
nsTextTransformer::GetNextWord(PRInt32& aWordLenResult,
                               PRInt32& aContentLenResult,
                               PRBool& aIsWhitespaceResult)
{
  NS_PRECONDITION(mOffset <= mContentLength, "bad offset");

  // See if the content has been exhausted
  if (mOffset == mContentLength) {
    aWordLenResult = 0;
    aContentLenResult = 0;
    return nsnull;
  }

  PRUnichar* bp = mBuffer;
  PRUnichar* bufEnd = mBuffer + mBufferLength;

  const nsTextFragment* frag = mCurrentFrag;
  const nsTextFragment* lastFrag = frag + mNumFrags;

  // Set the isWhitespace flag by examining the next character in the
  // text fragment.
  PRInt32 offset = mCurrentFragOffset;
  PRUnichar firstChar;
  if (frag->Is2b()) {
    const PRUnichar* up = frag->Get2b();
    firstChar = up[offset];
  }
  else {
    const unsigned char* cp = (const unsigned char*) frag->Get1b();
    firstChar = PRUnichar(cp[offset]);
  }
  PRBool isWhitespace = XP_IS_SPACE(firstChar);
  offset++;
  if (isWhitespace || (CH_NBSP == firstChar)) {
    *bp++ = ' ';
  }
  else {
    *bp++ = firstChar;
  }
  PRInt32 contentLen = 1;
  PRInt32 wordLen = 1;
  if (offset == frag->GetLength()) {
    mCurrentFrag = ++frag;
    offset = 0;
  }
  mCurrentFragOffset = offset;

  // XXX here is where we switch out to type specific handlers; this
  // code assumes mCompressWS is PR_TRUE

  // Now that we know what we are looking for, scan through the text
  // content and find the end of it.
  PRInt32 numChars;
  while (frag < lastFrag) {
    PRInt32 fragLen = frag->GetLength();

    // Scan characters in this fragment that are the same kind as the
    // isWhitespace flag indicates.
    if (frag->Is2b()) {
      const PRUnichar* cp0 = frag->Get2b();
      const PRUnichar* end = cp0 + fragLen;
      const PRUnichar* cp = cp0 + offset;
      if (isWhitespace) {
        while (cp < end) {
          PRUnichar ch = *cp;
          if (XP_IS_SPACE(ch)) {
            cp++;
            continue;
          }
          numChars = (cp - offset) - cp0;
          contentLen += numChars;
          mCurrentFragOffset += numChars;
          goto done;
        }
        numChars = (cp - offset) - cp0;
        contentLen += numChars;
      }
      else {
        while (cp < end) {
          PRUnichar ch = *cp;
          if (!XP_IS_SPACE(ch)) {
            if (CH_NBSP == ch) ch = ' ';
            if (ch > MAX_UNIBYTE) mHasMultibyte = PR_TRUE;
            cp++;
            // Store character in buffer; grow buffer if we have to
            *bp++ = ch;
            if (bp == bufEnd) {
              PRInt32 delta = bp - mBuffer;
              if (!GrowBuffer()) {
                goto done;
              }
              bp = mBuffer + delta;
              bufEnd = mBuffer + mBufferLength;
            }
            continue;
          }
          numChars = (cp - offset) - cp0;
          wordLen += numChars;
          contentLen += numChars;
          mCurrentFragOffset += numChars;
          goto done;
        }
        numChars = (cp - offset) - cp0;
        wordLen += numChars;
        contentLen += numChars;
      }
    }
    else {
      const unsigned char* cp0 = (const unsigned char*) frag->Get1b();
      const unsigned char* end = cp0 + fragLen;
      const unsigned char* cp = cp0 + offset;
      if (isWhitespace) {
        while (cp < end) {
          PRUnichar ch = PRUnichar(*cp);
          if (XP_IS_SPACE(ch)) {
            cp++;
            continue;
          }
          numChars = (cp - offset) - cp0;
          contentLen += numChars;
          mCurrentFragOffset += numChars;
          goto done;
        }
        numChars = (cp - offset) - cp0;
        contentLen += numChars;
      }
      else {
        while (cp < end) {
          PRUnichar ch = PRUnichar(*cp);
          if (!XP_IS_SPACE(ch)) {
            if (CH_NBSP == ch) ch = ' ';
            if (ch > MAX_UNIBYTE) mHasMultibyte = PR_TRUE;
            cp++;
            // Store character in buffer; grow buffer if we have to
            *bp++ = ch;
            if (bp == bufEnd) {
              PRInt32 delta = bp - mBuffer;
              if (!GrowBuffer()) {
                goto done;
              }
              bp = mBuffer + delta;
              bufEnd = mBuffer + mBufferLength;
            }
            continue;
          }
          numChars = (cp - offset) - cp0;
          wordLen += numChars;
          contentLen += numChars;
          mCurrentFragOffset += numChars;
          goto done;
        }
        numChars = (cp - offset) - cp0;
        wordLen += numChars;
        contentLen += numChars;
      }
    }

    // Advance to next text fragment
    frag++;
    mCurrentFrag = frag;
    mCurrentFragOffset = 0;
    offset = 0;
  }

 done:;
  mOffset += contentLen;
  NS_ASSERTION(mOffset <= mContentLength, "whoops");
  aWordLenResult = wordLen;
  aContentLenResult = contentLen;
  aIsWhitespaceResult = isWhitespace;
  return mBuffer;
}

PRUnichar*
nsTextTransformer::GetNextSection(PRInt32& aLineLenResult,
                                  PRInt32& aContentLenResult)
{
  NS_PRECONDITION(mOffset <= mContentLength, "bad offset");

  // See if the content has been exhausted
  if (mOffset == mContentLength) {
    aLineLenResult = 0;
    aContentLenResult = 0;
    return nsnull;
  }

  PRUnichar* bp = mBuffer;
  PRUnichar* bufEnd = mBuffer + mBufferLength;

  const nsTextFragment* frag = mCurrentFrag;
  const nsTextFragment* lastFrag = frag + mNumFrags;
  PRInt32 contentLen = 0;
  PRInt32 lineLen = 0;

  while (frag < lastFrag) {
    PRInt32 fragLen = frag->GetLength();

    // Scan characters in the fragment until we run out or we hit a
    // newline.
    if (frag->Is2b()) {
      const PRUnichar* cp0 = frag->Get2b();
      const PRUnichar* end = cp0 + fragLen;
      const PRUnichar* cp = cp0 + mCurrentFragOffset;
      while (cp < end) {
        PRUnichar ch = *cp++;
        if ('\t' == ch) {
          // Keep tabs seperate from other content
          if (0 != lineLen) {
            goto done;
          }
          *bp++ = ch;
          lineLen++;
          contentLen++;
          mCurrentFragOffset++;
          goto done;
        }
        else if ('\n' == ch) {
          // Keep newlines seperate from other content
          if (0 != lineLen) {
            goto done;
          }
          // Include newline in content-len but not in line-len
          contentLen++;
          mCurrentFragOffset++;
          goto done;
        }
        else if (CH_NBSP == ch) {
          ch = ' ';
        }
        if (ch > MAX_UNIBYTE) mHasMultibyte = PR_TRUE;
        *bp++ = ch;
        if (bp == bufEnd) {
          PRInt32 delta = bp - mBuffer;
          if (!GrowBuffer()) {
            goto done;
          }
          bp = mBuffer + delta;
          bufEnd = mBuffer + mBufferLength;
        }
        lineLen++;
        contentLen++;
        mCurrentFragOffset++;
      }
    }
    else {
      const unsigned char* cp0 = (const unsigned char*) frag->Get1b();
      const unsigned char* end = cp0 + fragLen;
      const unsigned char* cp = cp0 + mCurrentFragOffset;
      while (cp < end) {
        PRUnichar ch = PRUnichar(*cp++);
        if ('\t' == ch) {
          // Keep tabs seperate from other content
          if (0 != lineLen) {
            goto done;
          }
          *bp++ = ch;
          lineLen++;
          contentLen++;
          mCurrentFragOffset++;
          goto done;
        }
        else if ('\n' == ch) {
          // Keep newlines seperate from other content
          if (0 != lineLen) {
            goto done;
          }
          // Include newline in content-len but not in line-len
          contentLen++;
          mCurrentFragOffset++;
          goto done;
        }
        else if (CH_NBSP == ch) {
          ch = ' ';
        }
        if (ch > MAX_UNIBYTE) mHasMultibyte = PR_TRUE;
        *bp++ = ch;
        if (bp == bufEnd) {
          PRInt32 delta = bp - mBuffer;
          if (!GrowBuffer()) {
            goto done;
          }
          bp = mBuffer + delta;
          bufEnd = mBuffer + mBufferLength;
        }
        lineLen++;
        contentLen++;
        mCurrentFragOffset++;
      }
    }

    // Advance to next text fragment
    frag++;
    mCurrentFrag = frag;
    mCurrentFragOffset = 0;
  }

 done:;
  mOffset += contentLen;
  NS_ASSERTION(mOffset <= mContentLength, "whoops");
  aLineLenResult = lineLen;
  aContentLenResult = contentLen;
  return mBuffer;
}

PRUnichar*
nsTextTransformer::GetTextAt(PRInt32 aOffset)
{
  // XXX
  return mBuffer + aOffset;
}
