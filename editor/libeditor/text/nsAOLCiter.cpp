/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsAOLCiter.h"

#include "nsString.h"

/** Mail citations using the AOL style >> This is a citation <<
  */

nsAOLCiter::nsAOLCiter()
{
}

nsAOLCiter::~nsAOLCiter()
{
}

NS_IMPL_ADDREF(nsAOLCiter)

NS_IMPL_RELEASE(nsAOLCiter)

NS_IMETHODIMP
nsAOLCiter::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID()))
  {
    *aInstancePtr = (void*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsICiter::GetIID())) {
    *aInstancePtr = (void*)(nsICiter*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP
nsAOLCiter::GetCiteString(const nsString& aInString, nsString& aOutString)
{
  aOutString = "\n\n>> ";
  aOutString += aInString;

  // See if the last char is a newline, and replace it if so
  PRUnichar newline ('\n');
  if (aOutString.Last() == newline)
  {
    aOutString.SetCharAt(' ',aOutString.Length());
    aOutString += "<<\n";
  }
  else
  {
    aOutString += " <<\n";
  }

  return NS_OK;
}





