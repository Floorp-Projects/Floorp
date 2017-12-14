/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InternetCiter.h"

#include "nsAString.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsDebug.h"
#include "nsDependentSubstring.h"
#include "nsError.h"
#include "mozilla/intl/LineBreaker.h"
#include "nsLWBrkCIID.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsStringIterator.h"

namespace mozilla {

const char16_t gt ('>');
const char16_t space (' ');
const char16_t nl ('\n');
const char16_t cr('\r');

/**
 * Mail citations using the Internet style: > This is a citation.
 */

nsresult
InternetCiter::GetCiteString(const nsAString& aInString,
                             nsAString& aOutString)
{
  aOutString.Truncate();
  char16_t uch = nl;

  // Strip trailing new lines which will otherwise turn up
  // as ugly quoted empty lines.
  nsReadingIterator <char16_t> beginIter,endIter;
  aInString.BeginReading(beginIter);
  aInString.EndReading(endIter);
  while(beginIter!= endIter &&
        (*endIter == cr || *endIter == nl)) {
    --endIter;
  }

  // Loop over the string:
  while (beginIter != endIter) {
    if (uch == nl) {
      aOutString.Append(gt);
      // No space between >: this is ">>> " style quoting, for
      // compatibility with RFC 2646 and format=flowed.
      if (*beginIter != gt) {
        aOutString.Append(space);
      }
    }

    uch = *beginIter;
    ++beginIter;

    aOutString += uch;
  }

  if (uch != nl) {
    aOutString += nl;
  }
  return NS_OK;
}

nsresult
InternetCiter::StripCitesAndLinebreaks(const nsAString& aInString,
                                       nsAString& aOutString,
                                       bool aLinebreaksToo,
                                       int32_t* aCiteLevel)
{
  if (aCiteLevel) {
    *aCiteLevel = 0;
  }

  aOutString.Truncate();
  nsReadingIterator <char16_t> beginIter,endIter;
  aInString.BeginReading(beginIter);
  aInString.EndReading(endIter);
  while (beginIter!= endIter) { // loop over lines
    // Clear out cites first, at the beginning of the line:
    int32_t thisLineCiteLevel = 0;
    while (beginIter!= endIter &&
           (*beginIter == gt || nsCRT::IsAsciiSpace(*beginIter))) {
      if (*beginIter == gt) {
        ++thisLineCiteLevel;
      }
      ++beginIter;
    }
    // Now copy characters until line end:
    while (beginIter != endIter && (*beginIter != '\r' && *beginIter != '\n')) {
      aOutString.Append(*beginIter);
      ++beginIter;
    }
    if (aLinebreaksToo) {
      aOutString.Append(char16_t(' '));
    } else {
      aOutString.Append(char16_t('\n'));    // DOM linebreaks, not NS_LINEBREAK
    }
    // Skip over any more consecutive linebreak-like characters:
    while (beginIter != endIter && (*beginIter == '\r' || *beginIter == '\n')) {
      ++beginIter;
    }
    // Done with this line -- update cite level
    if (aCiteLevel && (thisLineCiteLevel > *aCiteLevel)) {
      *aCiteLevel = thisLineCiteLevel;
    }
  }
  return NS_OK;
}

nsresult
InternetCiter::StripCites(const nsAString& aInString,
                          nsAString& aOutString)
{
  return StripCitesAndLinebreaks(aInString, aOutString, false, 0);
}

static void AddCite(nsAString& aOutString, int32_t citeLevel)
{
  for (int32_t i = 0; i < citeLevel; ++i) {
    aOutString.Append(gt);
  }
  if (citeLevel > 0) {
    aOutString.Append(space);
  }
}

static inline void
BreakLine(nsAString& aOutString, uint32_t& outStringCol,
          uint32_t citeLevel)
{
  aOutString.Append(nl);
  if (citeLevel > 0) {
    AddCite(aOutString, citeLevel);
    outStringCol = citeLevel + 1;
  } else {
    outStringCol = 0;
  }
}

static inline bool IsSpace(char16_t c)
{
  const char16_t nbsp (0xa0);
  return (nsCRT::IsAsciiSpace(c) || (c == nl) || (c == cr) || (c == nbsp));
}

nsresult
InternetCiter::Rewrap(const nsAString& aInString,
                      uint32_t aWrapCol,
                      uint32_t aFirstLineOffset,
                      bool aRespectNewlines,
                      nsAString& aOutString)
{
  // There shouldn't be returns in this string, only dom newlines.
  // Check to make sure:
#ifdef DEBUG
  int32_t cr = aInString.FindChar(char16_t('\r'));
  NS_ASSERTION((cr < 0), "Rewrap: CR in string gotten from DOM!\n");
#endif /* DEBUG */

  aOutString.Truncate();

  nsresult rv;

  RefPtr<mozilla::intl::LineBreaker> lineBreaker =
    mozilla::intl::LineBreaker::Create();
  MOZ_ASSERT(lineBreaker);

  // Loop over lines in the input string, rewrapping each one.
  uint32_t length;
  uint32_t posInString = 0;
  uint32_t outStringCol = 0;
  uint32_t citeLevel = 0;
  const nsPromiseFlatString &tString = PromiseFlatString(aInString);
  length = tString.Length();
  while (posInString < length) {
    // Get the new cite level here since we're at the beginning of a line
    uint32_t newCiteLevel = 0;
    while (posInString < length && tString[posInString] == gt) {
      ++newCiteLevel;
      ++posInString;
      while (posInString < length && tString[posInString] == space) {
        ++posInString;
      }
    }
    if (posInString >= length) {
      break;
    }

    // Special case: if this is a blank line, maintain a blank line
    // (retain the original paragraph breaks)
    if (tString[posInString] == nl && !aOutString.IsEmpty()) {
      if (aOutString.Last() != nl) {
        aOutString.Append(nl);
      }
      AddCite(aOutString, newCiteLevel);
      aOutString.Append(nl);

      ++posInString;
      outStringCol = 0;
      continue;
    }

    // If the cite level has changed, then start a new line with the
    // new cite level (but if we're at the beginning of the string,
    // don't bother).
    if (newCiteLevel != citeLevel && posInString > newCiteLevel+1 &&
        outStringCol) {
      BreakLine(aOutString, outStringCol, 0);
    }
    citeLevel = newCiteLevel;

    // Prepend the quote level to the out string if appropriate
    if (!outStringCol) {
      AddCite(aOutString, citeLevel);
      outStringCol = citeLevel + (citeLevel ? 1 : 0);
    }
    // If it's not a cite, and we're not at the beginning of a line in
    // the output string, add a space to separate new text from the
    // previous text.
    else if (outStringCol > citeLevel) {
      aOutString.Append(space);
      ++outStringCol;
    }

    // find the next newline -- don't want to go farther than that
    int32_t nextNewline = tString.FindChar(nl, posInString);
    if (nextNewline < 0) {
      nextNewline = length;
    }

    // For now, don't wrap unquoted lines at all.
    // This is because the plaintext edit window has already wrapped them
    // by the time we get them for rewrap, yet when we call the line
    // breaker, it will refuse to break backwards, and we'll end up
    // with a line that's too long and gets displayed as a lone word
    // on a line by itself.  Need special logic to detect this case
    // and break it ourselves without resorting to the line breaker.
    if (!citeLevel) {
      aOutString.Append(Substring(tString, posInString,
                                  nextNewline-posInString));
      outStringCol += nextNewline - posInString;
      if (nextNewline != (int32_t)length) {
        aOutString.Append(nl);
        outStringCol = 0;
      }
      posInString = nextNewline+1;
      continue;
    }

    // Otherwise we have to use the line breaker and loop
    // over this line of the input string to get all of it:
    while ((int32_t)posInString < nextNewline) {
      // Skip over initial spaces:
      while ((int32_t)posInString < nextNewline &&
             nsCRT::IsAsciiSpace(tString[posInString])) {
        ++posInString;
      }

      // If this is a short line, just append it and continue:
      if (outStringCol + nextNewline - posInString <= aWrapCol-citeLevel-1) {
        // If this short line is the final one in the in string,
        // then we need to include the final newline, if any:
        if (nextNewline+1 == (int32_t)length && tString[nextNewline-1] == nl) {
          ++nextNewline;
        }
        // Trim trailing spaces:
        int32_t lastRealChar = nextNewline;
        while ((uint32_t)lastRealChar > posInString &&
               nsCRT::IsAsciiSpace(tString[lastRealChar-1])) {
          --lastRealChar;
        }

        aOutString += Substring(tString,
                                posInString, lastRealChar - posInString);
        outStringCol += lastRealChar - posInString;
        posInString = nextNewline + 1;
        continue;
      }

      int32_t eol = posInString + aWrapCol - citeLevel - outStringCol;
      // eol is the prospective end of line.
      // We'll first look backwards from there for a place to break.
      // If it's already less than our current position,
      // then our line is already too long, so break now.
      if (eol <= (int32_t)posInString) {
        BreakLine(aOutString, outStringCol, citeLevel);
        continue;    // continue inner loop, with outStringCol now at bol
      }

      int32_t breakPt = 0;
      // XXX Why this uses NS_ERROR_"BASE"?
      rv = NS_ERROR_BASE;
      if (lineBreaker) {
        breakPt = lineBreaker->Prev(tString.get() + posInString,
                                 length - posInString, eol + 1 - posInString);
        if (breakPt == NS_LINEBREAKER_NEED_MORE_TEXT) {
          // if we couldn't find a breakpoint looking backwards,
          // and we're not starting a new line, then end this line
          // and loop around again:
          if (outStringCol > citeLevel + 1) {
            BreakLine(aOutString, outStringCol, citeLevel);
            continue;    // continue inner loop, with outStringCol now at bol
          }

          // Else try looking forwards:
          breakPt = lineBreaker->Next(tString.get() + posInString,
                                      length - posInString, eol - posInString);

          rv = breakPt == NS_LINEBREAKER_NEED_MORE_TEXT ? NS_ERROR_BASE :
                                                          NS_OK;
        } else {
          rv = NS_OK;
        }
      }
      // If rv is okay, then breakPt is the place to break.
      // If we get out here and rv is set, something went wrong with line
      // breaker.  Just break the line, hard.
      if (NS_FAILED(rv)) {
        breakPt = eol;
      }

      // Special case: maybe we should have wrapped last time.
      // If the first breakpoint here makes the current line too long,
      // then if we already have text on the current line,
      // break and loop around again.
      // If we're at the beginning of the current line, though,
      // don't force a break since the long word might be a url
      // and breaking it would make it unclickable on the other end.
      const int SLOP = 6;
      if (outStringCol + breakPt > aWrapCol + SLOP &&
          outStringCol > citeLevel+1) {
        BreakLine(aOutString, outStringCol, citeLevel);
        continue;
      }

      nsAutoString sub (Substring(tString, posInString, breakPt));
      // skip newlines or whitespace at the end of the string
      int32_t subend = sub.Length();
      while (subend > 0 && IsSpace(sub[subend-1])) {
        --subend;
      }
      sub.Left(sub, subend);
      aOutString += sub;
      outStringCol += sub.Length();
      // Advance past the whitespace which caused the wrap:
      posInString += breakPt;
      while (posInString < length && IsSpace(tString[posInString])) {
        ++posInString;
      }

      // Add a newline and the quote level to the out string
      if (posInString < length) {  // not for the last line, though
        BreakLine(aOutString, outStringCol, citeLevel);
      }
    } // end inner loop within one line of aInString
  } // end outer loop over lines of aInString

  return NS_OK;
}

} // namespace mozilla
