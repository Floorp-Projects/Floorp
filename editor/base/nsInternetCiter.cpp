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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */


#include "nsString.h"
#include "nsInternetCiter.h"

#include "nsCOMPtr.h"

// Line breaker stuff
#include "nsIServiceManager.h"
#include "nsILineBreakerFactory.h"
#include "nsLWBrkCIID.h"

static PRUnichar gt ('>');
static PRUnichar space (' ');
static PRUnichar nl ('\n');
static PRUnichar cr('\r');

/** Mail citations using the Internet style: > This is a citation
  */

static NS_DEFINE_CID(kLWBrkCID, NS_LWBRK_CID);

nsInternetCiter::nsInternetCiter()
{
  NS_INIT_REFCNT();
}

nsInternetCiter::~nsInternetCiter()
{
}

NS_IMPL_ADDREF(nsInternetCiter)

NS_IMPL_RELEASE(nsInternetCiter)

NS_IMETHODIMP
nsInternetCiter::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(NS_GET_IID(nsISupports)))
  {
    *aInstancePtr = (void*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsICiter))) {
    *aInstancePtr = (void*)(nsICiter*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP
nsInternetCiter::GetCiteString(const nsAReadableString& aInString, nsAWritableString& aOutString)
{
  aOutString.SetLength(0);
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
      // compatability with RFC 2646 and format=flowed.
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
nsInternetCiter::StripCitesAndLinebreaks(const nsAReadableString& aInString,
                                         nsAWritableString& aOutString,
                                         PRBool aLinebreaksToo,
                                         PRInt32* aCiteLevel)
{
  if (aCiteLevel)
    *aCiteLevel = 0;

  aOutString.SetLength(0);
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

NS_IMETHODIMP
nsInternetCiter::StripCites(const nsAReadableString& aInString, nsAWritableString& aOutString)
{
  return StripCitesAndLinebreaks(aInString, aOutString, PR_FALSE, 0);
}

static void AddCite(nsAWritableString& aOutString, PRInt32 citeLevel)
{
  for (PRInt32 i = 0; i < citeLevel; ++i)
    aOutString.Append(PRUnichar(gt));
  if (citeLevel > 0)
    aOutString.Append(PRUnichar(space));
}

NS_IMETHODIMP
nsInternetCiter::Rewrap(const nsAReadableString& aInString,
                        PRUint32 aWrapCol, PRUint32 aFirstLineOffset,
                        PRBool aRespectNewlines,
                        nsAWritableString& aOutString)
{
  // There shouldn't be returns in this string, only dom newlines.
  // Check to make sure:
#ifdef DEBUG
  PRInt32 cr = aInString.FindChar(PRUnichar('\r'));
  NS_ASSERTION((cr < 0), "Rewrap: CR in string gotten from DOM!\n");
#endif /* DEBUG */

  nsCOMPtr<nsILineBreaker> lineBreaker;
  nsILineBreakerFactory *lf;
  nsresult rv = NS_OK;
  rv = nsServiceManager::GetService(kLWBrkCID,
                                    NS_GET_IID(nsILineBreakerFactory),
                                    (nsISupports **)&lf);
  if (NS_SUCCEEDED(rv))
  {
    nsAutoString lbarg;
    rv = lf->GetBreaker(lbarg, getter_AddRefs(lineBreaker));
    nsServiceManager::ReleaseService(kLWBrkCID, lf);
  }

  aOutString.SetLength(0);

  // Loop over lines in the input string, rewrapping each one.
  PRUint32 length = aInString.Length();
  PRUint32 posInString = 0;
  PRUint32 outStringCol = 0;
  PRUint32 citeLevel = 0;
  const nsPromiseFlatString &tString = PromiseFlatString(aInString);//MJUDGE SCC NEED HELP
  while (posInString < length)
  {
#ifdef DEBUG_wrapping
    nsAutoString debug (Substring(tString, posInString, length-posInString));
    printf("Outer loop: '%s'\n", debug.ToNewCString());
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
    if (tString[posInString] == nl && aOutString.Length() > 0)
    {
      nsReadingIterator <PRUnichar> outPeekIter;
      aOutString.EndReading(outPeekIter);
      outPeekIter.advance(-1);
      if ((*outPeekIter) != nl)
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
      //aOutString.Append(nl);
      //AddCite(aOutString, citeLevel);
      aOutString.Append(nl);
      outStringCol = 0;
    }
    citeLevel = newCiteLevel;

    // Prepend the quote level to the out string if appropriate
    if (outStringCol == 0)
    {
      AddCite(aOutString, citeLevel);
      outStringCol = citeLevel;
    }
    // If it's not a cite, and we're not at the beginning of a line in
    // the output string, add a space to separate new text from the
    // previous text.
    else if (outStringCol > 0)
    {
#ifdef DEBUG_wrapping
      printf("Appending space; citeLevel=%d, outStringCol=%d\n", citeLevel,
             outStringCol);
#endif
      aOutString.Append(PRUnichar(space));
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
#ifdef DEBUG_wrapping
      nsAutoString debug (Substring(tString, posInString,
                                    nextNewline-posInString));
      printf("Unquoted: appending '%s'\n", debug.ToNewCString());
#endif
      aOutString.Append(Substring(tString, posInString,
                                  nextNewline-posInString));
      outStringCol += nextNewline - posInString;
      if (nextNewline != (PRInt32)length)
      {
#ifdef DEBUG_wrapping
        printf("unquoted: appending a newline\n");
#endif
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
      nsAutoString debug (Substring(tString, posInString, nextNewline-posInString));
      printf("Inner loop: '%s'\n", debug.ToNewCString());
#endif

      // If this is a short line, just append it and continue:
      if (outStringCol + nextNewline - posInString <= aWrapCol-citeLevel-1)
      {
        // If this short line is the final one in the in string,
        // then we need to include the final newline, if any:
        if (nextNewline+1 == (PRInt32)length && tString[nextNewline-1] == nl)
          ++nextNewline;
#ifdef DEBUG_wrapping
        nsAutoString debug (Substring(tString, posInString, nextNewline - posInString));
        printf("Short line: '%s'\n", debug.ToNewCString());
#endif
        aOutString += Substring(tString,
                                posInString, nextNewline - posInString);
        outStringCol += nextNewline - posInString;
        posInString = nextNewline + 1;
        continue;
      }

      PRInt32 eol = posInString + aWrapCol - citeLevel - 1 - outStringCol;
      // eol is the prospective end of line ...
      // first look backwards from there for a place to break.
      PRUint32 breakPt;
      PRBool needMore;
      rv = NS_ERROR_BASE;
      if (lineBreaker)
      {
        rv = lineBreaker->Prev(tString.get() + posInString, length - posInString,
                               eol - posInString, &breakPt, &needMore);
        if (NS_FAILED(rv) || needMore)
        {
          // if we couldn't find a breakpoint looking backwards,
          // try looking forwards:
          rv = lineBreaker->Next(tString.get() + posInString,
                                 length - posInString,
                                 eol - posInString, &breakPt, &needMore);
          if (needMore) rv = NS_ERROR_BASE;
        }
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
#ifdef DEBUG_wrapping
      printf("breakPt = %d\n", breakPt);
#endif

      aOutString += Substring(tString, posInString, breakPt);
      posInString += breakPt;
      outStringCol += breakPt;

      // Add a newline and the quote level to the out string
      if (posInString < length)    // not for the last line, though
      {
        aOutString.Append(nl);
        AddCite(aOutString, citeLevel);
        outStringCol = citeLevel + (citeLevel ? 1 : 0);
      }
    } // end inner loop within one line of aInString
#ifdef DEBUG_wrapping
    printf("---------\nEnd inner loop: out string is now '%s'\n-----------\n",
           aOutString.ToNewCString());
#endif
  } // end outer loop over lines of aInString

  return NS_OK;
}

