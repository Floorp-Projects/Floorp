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

/** Mail citations using the Internet style >> This is a citation <<
  */

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
  PRInt32 i = 0;
  PRInt32 length = aInString.Length();
  aOutString.SetLength(0);
  PRUnichar uch = newline;

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

NS_IMETHODIMP
nsInternetCiter::StripCites(const nsString& aInString, nsString& aOutString)
{
  aOutString.SetLength(0);

  PRInt32 length = aInString.Length();
  PRInt32 i = 0;
  PRUnichar gt ('>');
  while (i < length)  // loop over lines
  {
    while (aInString[i] == gt || nsCRT::IsAsciiSpace(aInString[i]))
      ++i;
    PRInt32 nextNewline = aInString.FindCharInSet("\r\n", i);
    if (nextNewline > i)
    {
      while (i < nextNewline)
        aOutString += aInString[i++];
      aOutString += NS_LINEBREAK;
      while (aOutString[i] == '\r' || aOutString[i] == '\n')
        ++i;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsInternetCiter::Rewrap(const nsString& aInString,
                        PRUint32 aWrapCol, PRUint32 aFirstLineOffset,
                        nsString& aOutString)
{
  printf("nsInternetCiter::Rewrap not yet implemented\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

