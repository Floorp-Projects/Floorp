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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
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


#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsInternetCiter.h"
#include "nsCRT.h"

#include "nsCOMPtr.h"

// Line breaker stuff
#include "nsIServiceManager.h"
#include "nsLWBrkCIID.h"
#include "nsILineBreaker.h"

const PRUnichar gt ('>');
const PRUnichar space (' ');
const PRUnichar nbsp (0xa0);
const PRUnichar nl ('\n');
const PRUnichar cr('\r');

/** Mail citations using the Internet style: > This is a citation
  */

nsresult
nsInternetCiter::GetCiteString(const nsAString& aInString, nsAString& aOutString)
{
  aOutString.Truncate();
  PRUnichar uch = nl;

  // Strip trailing new lines which will otherwise turn up
  // as ugly quoted empty lines.
  nsReadingIterator <PRUnichar> beginIter,endIter;
  aInString.BeginReading(beginIter);
  aInString.EndReading(endIter);
  while(beginIter!= endIter &&
        (*endIter == cr ||
         *endIter == nl))
  {
    --endIter;
  }

  // Loop over the string:
  while (beginIter != endIter)
  {
    if (uch == nl)
    {
      aOutString.Append(gt);
      // No space between >: this is ">>> " style quoting, for
      // compatibility with RFC 2646 and format=flowed.
      if (*beginIter != gt)
        aOutString.Append(space);
    }

    uch = *beginIter;
    ++beginIter;

    aOutString += uch;
  }

  if (uch != nl)
    aOutString += nl;

  return NS_OK;
}

nsresult
nsInternetCiter::StripCitesAndLinebreaks(const nsAString& aInString,
                                         nsAString& aOutString,
                                         PRBool aLinebreaksToo,
                                         PRInt32* aCiteLevel)
{
  if (aCiteLevel)
    *aCiteLevel = 0;

  aOutString.Truncate();
  nsReadingIterator <PRUnichar> beginIter,endIter;
  aInString.BeginReading(beginIter);
  aInString.EndReading(endIter);
  while (beginIter!= endIter)  // loop over lines
  {
    // Clear out cites first, at the beginning of the line:
    PRInt32 thisLineCiteLevel = 0;
    while (beginIter!= endIter && (*beginIter == gt || nsCRT::IsAsciiSpace(*beginIter)))
    {
      if (*beginIter == gt) ++thisLineCiteLevel;
      ++beginIter;
    }

    // Now copy characters until line end:
    while (beginIter != endIter && (*beginIter != '\r' && *beginIter != '\n'))
    {
      aOutString.Append(*beginIter);
      ++beginIter;
    }
    if (aLinebreaksToo)
      aOutString.Append(PRUnichar(' '));
    else
      aOutString.Append(PRUnichar('\n'));    // DOM linebreaks, not NS_LINEBREAK
      // Skip over any more consecutive linebreak-like characters:
    while (beginIter != endIter && (*beginIter == '\r' || *beginIter == '\n'))
      ++beginIter;

    // Done with this line -- update cite level
    if (aCiteLevel && (thisLineCiteLevel > *aCiteLevel))
      *aCiteLevel = thisLineCiteLevel;
  }
  return NS_OK;
}

nsresult
nsInternetCiter::StripCites(const nsAString& aInString, nsAString& aOutString)
{
  return StripCitesAndLinebreaks(aInString, aOutString, PR_FALSE, 0);
}

static void AddCite(nsAString& aOutString, PRInt32 citeLevel)
{
  for (PRInt32 i = 0; i < citeLevel; ++i)
    aOutString.Append(gt);
  if (citeLevel > 0)
    aOutString.Append(space);
}

static inline void
BreakLine(nsAString& aOutString, PRUint32& outStringCol,
          PRUint32 citeLevel)
{
  aOutString.Append(nl);
  if (citeLevel > 0)
  {
    AddCite(aOutString, citeLevel);
    outStringCol = citeLevel + 1;
  }
  else
    outStringCol = 0;
}

static inline PRBool IsSpace(PRUnichar c)
{
  return (nsCRT::IsAsciiSpace(c) || (c == nl) || (c == cr) || (c == nbsp));
}

nsresult
nsInternetCiter::Rewrap(const nsAString& aInString,
                        PRUint32 aWrapCol, PRUint32 aFirstLineOffset,
                        PRBool aRespectNewlines,
                        nsAString& aOutString)
{
  // There shouldn't be returns in this string, only dom newlines.
  // Check to make sure:
#ifdef DEBUG
  PRInt32 cr = aInString.FindChar(PRUnichar('\r'));
  NS_ASSERTION((cr < 0), "Rewrap: CR in string gotten from DOM!\n");
#endif /* DEBUG */

  aOutString.Truncate();

  nsresult rv;
  nsCOMPtr<nsILineBreaker> lineBreaker = do_GetService(NS_LBRK_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Loop over lines in the input string, rewrapping each one.
  PRUint32 length;
  PRUint32 posInString = 0;
  PRUint32 outStringCol = 0;
  PRUint32 citeLevel = 0;
  const nsPromiseFlatString &tString = PromiseFlatString(aInString);
  length = tString.Length();
#ifdef DEBUG_wrapping
  int loopcount = 0;
#endif
  while (posInString < length)
  {
#ifdef DEBUG_wrapping
    printf("Outer loop: '%s'\n",
           NS_LossyConvertUTF16toASCII(Substring(tString, posInString,
                                                length-posInString)).get());
    printf("out string is now: '%s'\n",
           NS_LossyConvertUTF16toASCII(aOutString).get());

#endif

    // Get the new cite level here since we're at the beginning of a line
    PRUint32 newCiteLevel = 0;
    while (posInString < length && tString[posInString] == gt)
    {
      ++newCiteLevel;
      ++posInString;
      while (posInString < length && tString[posInString] == space)
        ++posInString;
    }
    if (posInString >= length)
      break;

    // Special case: if this is a blank line, maintain a blank line
    // (retain the original paragraph breaks)
    if (tString[posInString] == nl && !aOutString.IsEmpty())
    {
      if (aOutString.Last() != nl)
        aOutString.Append(nl);
      AddCite(aOutString, newCiteLevel);
      aOutString.Append(nl);

      ++posInString;
      outStringCol = 0;
      continue;
    }

    // If the cite level has changed, then start a new line with the
    // new cite level (but if we're at the beginning of the string,
    // don't bother).
    if (newCiteLevel != citeLevel && posInString > newCiteLevel+1
        && outStringCol != 0)
    {
      BreakLine(aOutString, outStringCol, 0);
    }
    citeLevel = newCiteLevel;

    // Prepend the quote level to the out string if appropriate
    if (outStringCol == 0)
    {
      AddCite(aOutString, citeLevel);
      outStringCol = citeLevel + (citeLevel ? 1 : 0);
    }
    // If it's not a cite, and we're not at the beginning of a line in
    // the output string, add a space to separate new text from the
    // previous text.
    else if (outStringCol > citeLevel)
    {
      aOutString.Append(space);
      ++outStringCol;
    }

    // find the next newline -- don't want to go farther than that
    PRInt32 nextNewline = tString.FindChar(nl, posInString);
    if (nextNewline < 0) nextNewline = length;

    // For now, don't wrap unquoted lines at all.
    // This is because the plaintext edit window has already wrapped them
    // by the time we get them for rewrap, yet when we call the line
    // breaker, it will refuse to break backwards, and we'll end up
    // with a line that's too long and gets displayed as a lone word
    // on a line by itself.  Need special logic to detect this case
    // and break it ourselves without resorting to the line breaker.
    if (citeLevel == 0)
    {
      aOutString.Append(Substring(tString, posInString,
                                  nextNewline-posInString));
      outStringCol += nextNewline - posInString;
      if (nextNewline != (PRInt32)length)
      {
        aOutString.Append(nl);
        outStringCol = 0;
      }
      posInString = nextNewline+1;
      continue;
    }

    // Otherwise we have to use the line breaker and loop
    // over this line of the input string to get all of it:
    while ((PRInt32)posInString < nextNewline)
    {
#ifdef DEBUG_wrapping
      if (++loopcount > 1000)
        NS_ASSERTION(PR_FALSE, "possible infinite loop in nsInternetCiter\n");

      printf("Inner loop: '%s'\n",
             NS_LossyConvertUTF16toASCII(Substring(tString, posInString,
                                              nextNewline-posInString)).get());
#endif

      // Skip over initial spaces:
      while ((PRInt32)posInString < nextNewline
             && nsCRT::IsAsciiSpace(tString[posInString]))
        ++posInString;

      // If this is a short line, just append it and continue:
      if (outStringCol + nextNewline - posInString <= aWrapCol-citeLevel-1)
      {
        // If this short line is the final one in the in string,
        // then we need to include the final newline, if any:
        if (nextNewline+1 == (PRInt32)length && tString[nextNewline-1] == nl)
          ++nextNewline;

        // Trim trailing spaces:
        PRInt32 lastRealChar = nextNewline;
        while ((PRUint32)lastRealChar > posInString
               && nsCRT::IsAsciiSpace(tString[lastRealChar-1]))
          --lastRealChar;

        aOutString += Substring(tString,
                                posInString, lastRealChar - posInString);
        outStringCol += lastRealChar - posInString;
        posInString = nextNewline + 1;
        continue;
      }

      PRInt32 eol = posInString + aWrapCol - citeLevel - outStringCol;
      // eol is the prospective end of line.
      // We'll first look backwards from there for a place to break.
      // If it's already less than our current position,
      // then our line is already too long, so break now.
      if (eol <= (PRInt32)posInString)
      {
        BreakLine(aOutString, outStringCol, citeLevel);
        continue;    // continue inner loop, with outStringCol now at bol
      }

      PRInt32 breakPt = 0;
      rv = NS_ERROR_BASE;
      if (lineBreaker)
      {
        breakPt = lineBreaker->Prev(tString.get() + posInString,
                                 length - posInString, eol + 1 - posInString);
        if (breakPt == NS_LINEBREAKER_NEED_MORE_TEXT)
        {
          // if we couldn't find a breakpoint looking backwards,
          // and we're not starting a new line, then end this line
          // and loop around again:
          if (outStringCol > citeLevel + 1)
          {
            BreakLine(aOutString, outStringCol, citeLevel);
            continue;    // continue inner loop, with outStringCol now at bol
          }

          // Else try looking forwards:
          breakPt = lineBreaker->Next(tString.get() + posInString,
                                      length - posInString, eol - posInString);
          if (breakPt == NS_LINEBREAKER_NEED_MORE_TEXT) rv = NS_ERROR_BASE;
          else rv = NS_OK;
        }
        else rv = NS_OK;
      }
      // If rv is okay, then breakPt is the place to break.
      // If we get out here and rv is set, something went wrong with line
      // breaker.  Just break the line, hard.
      if (NS_FAILED(rv))
      {
#ifdef DEBUG_akkana
        printf("nsInternetCiter: LineBreaker not working -- breaking hard\n");
#endif
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
      if (outStringCol + breakPt > aWrapCol + SLOP
          && outStringCol > citeLevel+1)
      {
        BreakLine(aOutString, outStringCol, citeLevel);
        continue;
      }

      nsAutoString sub (Substring(tString, posInString, breakPt));
      // skip newlines or whitespace at the end of the string
      PRInt32 subend = sub.Length();
      while (subend > 0 && IsSpace(sub[subend-1]))
        --subend;
      sub.Left(sub, subend);
      aOutString += sub;
      outStringCol += sub.Length();
      // Advance past the whitespace which caused the wrap:
      posInString += breakPt;
      while (posInString < length && IsSpace(tString[posInString]))
        ++posInString;

      // Add a newline and the quote level to the out string
      if (posInString < length)    // not for the last line, though
        BreakLine(aOutString, outStringCol, citeLevel);

    } // end inner loop within one line of aInString
#ifdef DEBUG_wrapping
    printf("---------\nEnd inner loop: out string is now '%s'\n-----------\n",
           NS_LossyConvertUTF16toASCII(aOutString).get());
#endif
  } // end outer loop over lines of aInString

#ifdef DEBUG_wrapping
  printf("Final out string is now: '%s'\n",
         NS_LossyConvertUTF16toASCII(aOutString).get());

#endif
  return NS_OK;
}


