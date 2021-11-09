/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InternetCiter.h"

#include "mozilla/Casting.h"
#include "mozilla/intl/Segmenter.h"
#include "HTMLEditUtils.h"
#include "nsAString.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsDebug.h"
#include "nsDependentSubstring.h"
#include "nsError.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsStringIterator.h"

namespace mozilla {

/**
 * Mail citations using the Internet style: > This is a citation.
 */

void InternetCiter::GetCiteString(const nsAString& aInString,
                                  nsAString& aOutString) {
  aOutString.Truncate();
  char16_t uch = HTMLEditUtils::kNewLine;

  // Strip trailing new lines which will otherwise turn up
  // as ugly quoted empty lines.
  nsReadingIterator<char16_t> beginIter, endIter;
  aInString.BeginReading(beginIter);
  aInString.EndReading(endIter);
  while (beginIter != endIter && (*endIter == HTMLEditUtils::kCarriageReturn ||
                                  *endIter == HTMLEditUtils::kNewLine)) {
    --endIter;
  }

  // Loop over the string:
  while (beginIter != endIter) {
    if (uch == HTMLEditUtils::kNewLine) {
      aOutString.Append(HTMLEditUtils::kGreaterThan);
      // No space between >: this is ">>> " style quoting, for
      // compatibility with RFC 2646 and format=flowed.
      if (*beginIter != HTMLEditUtils::kGreaterThan) {
        aOutString.Append(HTMLEditUtils::kSpace);
      }
    }

    uch = *beginIter;
    ++beginIter;

    aOutString += uch;
  }

  if (uch != HTMLEditUtils::kNewLine) {
    aOutString += HTMLEditUtils::kNewLine;
  }
}

static void AddCite(nsAString& aOutString, int32_t citeLevel) {
  for (int32_t i = 0; i < citeLevel; ++i) {
    aOutString.Append(HTMLEditUtils::kGreaterThan);
  }
  if (citeLevel > 0) {
    aOutString.Append(HTMLEditUtils::kSpace);
  }
}

static inline void BreakLine(nsAString& aOutString, uint32_t& outStringCol,
                             uint32_t citeLevel) {
  aOutString.Append(HTMLEditUtils::kNewLine);
  if (citeLevel > 0) {
    AddCite(aOutString, citeLevel);
    outStringCol = citeLevel + 1;
  } else {
    outStringCol = 0;
  }
}

static inline bool IsSpace(char16_t c) {
  return (nsCRT::IsAsciiSpace(c) || (c == HTMLEditUtils::kNewLine) ||
          (c == HTMLEditUtils::kCarriageReturn) || (c == HTMLEditUtils::kNBSP));
}

void InternetCiter::Rewrap(const nsAString& aInString, uint32_t aWrapCol,
                           uint32_t aFirstLineOffset, bool aRespectNewlines,
                           nsAString& aOutString) {
  // There shouldn't be returns in this string, only dom newlines.
  // Check to make sure:
#ifdef DEBUG
  int32_t crPosition = aInString.FindChar(HTMLEditUtils::kCarriageReturn);
  NS_ASSERTION(crPosition < 0, "Rewrap: CR in string gotten from DOM!\n");
#endif /* DEBUG */

  aOutString.Truncate();

  // Loop over lines in the input string, rewrapping each one.
  uint32_t posInString = 0;
  uint32_t outStringCol = 0;
  uint32_t citeLevel = 0;
  const nsPromiseFlatString& tString = PromiseFlatString(aInString);
  const uint32_t length = tString.Length();
  while (posInString < length) {
    // Get the new cite level here since we're at the beginning of a line
    uint32_t newCiteLevel = 0;
    while (posInString < length &&
           tString[posInString] == HTMLEditUtils::kGreaterThan) {
      ++newCiteLevel;
      ++posInString;
      while (posInString < length &&
             tString[posInString] == HTMLEditUtils::kSpace) {
        ++posInString;
      }
    }
    if (posInString >= length) {
      break;
    }

    // Special case: if this is a blank line, maintain a blank line
    // (retain the original paragraph breaks)
    if (tString[posInString] == HTMLEditUtils::kNewLine &&
        !aOutString.IsEmpty()) {
      if (aOutString.Last() != HTMLEditUtils::kNewLine) {
        aOutString.Append(HTMLEditUtils::kNewLine);
      }
      AddCite(aOutString, newCiteLevel);
      aOutString.Append(HTMLEditUtils::kNewLine);

      ++posInString;
      outStringCol = 0;
      continue;
    }

    // If the cite level has changed, then start a new line with the
    // new cite level (but if we're at the beginning of the string,
    // don't bother).
    if (newCiteLevel != citeLevel && posInString > newCiteLevel + 1 &&
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
      aOutString.Append(HTMLEditUtils::kSpace);
      ++outStringCol;
    }

    // find the next newline -- don't want to go farther than that
    int32_t nextNewline =
        tString.FindChar(HTMLEditUtils::kNewLine, posInString);
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
      aOutString.Append(
          Substring(tString, posInString, nextNewline - posInString));
      outStringCol += nextNewline - posInString;
      if (nextNewline != (int32_t)length) {
        aOutString.Append(HTMLEditUtils::kNewLine);
        outStringCol = 0;
      }
      posInString = nextNewline + 1;
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
      if (outStringCol + nextNewline - posInString <=
          aWrapCol - citeLevel - 1) {
        // If this short line is the final one in the in string,
        // then we need to include the final newline, if any:
        if (nextNewline + 1 == (int32_t)length &&
            tString[nextNewline - 1] == HTMLEditUtils::kNewLine) {
          ++nextNewline;
        }
        // Trim trailing spaces:
        int32_t lastRealChar = nextNewline;
        while ((uint32_t)lastRealChar > posInString &&
               nsCRT::IsAsciiSpace(tString[lastRealChar - 1])) {
          --lastRealChar;
        }

        aOutString +=
            Substring(tString, posInString, lastRealChar - posInString);
        outStringCol += lastRealChar - posInString;
        posInString = nextNewline + 1;
        continue;
      }

      int32_t eol = posInString + aWrapCol - citeLevel - outStringCol;
      // eol is the prospective end of line.
      // If it's already less than our current position,
      // then our line is already too long, so break now.
      if (eol <= (int32_t)posInString) {
        BreakLine(aOutString, outStringCol, citeLevel);
        continue;  // continue inner loop, with outStringCol now at bol
      }

      MOZ_ASSERT(eol >= 0 && eol - posInString > 0);

      uint32_t breakPt = 0;
      Maybe<uint32_t> nextBreakPt;
      intl::LineBreakIteratorUtf16 lineBreakIter(Span<const char16_t>(
          tString.get() + posInString, length - posInString));
      while (true) {
        nextBreakPt = lineBreakIter.Next();
        if (!nextBreakPt ||
            *nextBreakPt > AssertedCast<uint32_t>(eol) - posInString) {
          break;
        }
        breakPt = *nextBreakPt;
      }

      if (breakPt == 0) {
        // If we couldn't find a breakpoint within the eol upper bound, and
        // we're not starting a new line, then end this line and loop around
        // again:
        if (outStringCol > citeLevel + 1) {
          BreakLine(aOutString, outStringCol, citeLevel);
          continue;  // continue inner loop, with outStringCol now at bol
        }

        MOZ_ASSERT(nextBreakPt.isSome(),
                   "Next() always treats end-of-text as a break");
        breakPt = *nextBreakPt;
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
          outStringCol > citeLevel + 1) {
        BreakLine(aOutString, outStringCol, citeLevel);
        continue;
      }

      nsAutoString sub(Substring(tString, posInString, breakPt));
      // skip newlines or white-space at the end of the string
      int32_t subend = sub.Length();
      while (subend > 0 && IsSpace(sub[subend - 1])) {
        --subend;
      }
      sub.Left(sub, subend);
      aOutString += sub;
      outStringCol += sub.Length();
      // Advance past the white-space which caused the wrap:
      posInString += breakPt;
      while (posInString < length && IsSpace(tString[posInString])) {
        ++posInString;
      }

      // Add a newline and the quote level to the out string
      if (posInString < length) {  // not for the last line, though
        BreakLine(aOutString, outStringCol, citeLevel);
      }
    }  // end inner loop within one line of aInString
  }    // end outer loop over lines of aInString
}

}  // namespace mozilla
