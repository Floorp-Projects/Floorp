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

#include "nsInternetCiter.h"

#include "nsString.h"

#include "nsWrapUtils.h"

#include "nsCOMPtr.h"

// Line breaker stuff
#include "nsIServiceManager.h"
#include "nsILineBreakerFactory.h"
#include "nsLWBrkCIID.h"

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
nsInternetCiter::GetCiteString(const nsString& aInString, nsString& aOutString)
{
  PRUnichar newline ('\n');      // Not XP!
  PRUnichar cr('\r');
  PRInt32 i = 0;
  PRInt32 length = aInString.Length();
  aOutString.SetLength(0);
  PRUnichar uch = newline;

  // Strip trailing new lines which will otherwise turn up
  // as ugly quoted empty lines.
  while(length > 0 &&
        (aInString[length-1] == cr ||
         aInString[length-1] == newline))
  {
    --length;
  }

  // Loop over the string:
  while (i < length)
  {
    if (uch == newline)
      aOutString.AppendWithConversion("> ");

    uch = aInString[i++];
    aOutString += uch;
  }

  if (uch != newline)
    aOutString += newline;

  return NS_OK;
}

nsresult
nsInternetCiter::StripCitesAndLinebreaks(const nsString& aInString,
                                         nsString& aOutString,
                                         PRBool aLinebreaksToo,
                                         PRInt32* aCiteLevel)
{
  if (aCiteLevel)
    *aCiteLevel = 0;

  aOutString.SetLength(0);

  PRInt32 length = aInString.Length();
  PRInt32 i = 0;
  PRUnichar gt ('>');
  while (i < length)  // loop over lines
  {
    // Clear out cites first, at the beginning of the line:
    PRInt32 thisLineCiteLevel = 0;
    while (aInString[i] == gt || nsCRT::IsAsciiSpace(aInString[i]))
    {
      if (aInString[i] == gt) ++thisLineCiteLevel;
      ++i;
    }

    // Now copy characters until line end:
    PRInt32 nextNewline = aInString.FindCharInSet("\r\n", i);
    if (nextNewline > i)
    {
      while (i < nextNewline)
        aOutString.Append(aInString[i++]);
      if (aLinebreaksToo)
        aOutString.AppendWithConversion(' ');
      else
        aOutString.AppendWithConversion('\n');    // DOM linebreaks, not NS_LINEBREAK
      // Skip over any more consecutive linebreak-like characters:
      while (aOutString[i] == '\r' || aOutString[i] == '\n')
        ++i;
    }
    else    // no more newlines
    {
      while (i < length)
        aOutString.Append(aInString[i++]);
    }

    // Done with this line -- update cite level
    if (aCiteLevel && (thisLineCiteLevel > *aCiteLevel))
      *aCiteLevel = thisLineCiteLevel;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsInternetCiter::StripCites(const nsString& aInString, nsString& aOutString)
{
  return StripCitesAndLinebreaks(aInString, aOutString, PR_FALSE, 0);
}

NS_IMETHODIMP
nsInternetCiter::Rewrap(const nsString& aInString,
                        PRUint32 aWrapCol, PRUint32 aFirstLineOffset,
                        PRBool aRespectNewlines,
                        nsString& aOutString)
{
  PRInt32 i;

  // First, clean up all the existing newlines/cite marks and save the cite level.
  nsAutoString inString;
  PRInt32 citeLevel;
  StripCitesAndLinebreaks(aInString, inString, !aRespectNewlines, &citeLevel);

  nsAutoString citeString;
  for (i=0; i<citeLevel; ++i)
    citeString.AppendWithConversion("> ");

  return nsWrapUtils::Rewrap(inString, aWrapCol, aFirstLineOffset,
                             aRespectNewlines, citeString,
                             aOutString);
}

