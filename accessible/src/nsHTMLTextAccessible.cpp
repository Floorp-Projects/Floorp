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
 * Author: Eric Vaughan (evaughan@netscape.com)
 * Contributor(s): 
 */

#include "nsHTMLTextAccessible.h"
#include "nsICheckboxControlFrame.h"
#include "nsWeakReference.h"
#include "nsIFrame.h"

nsHTMLTextAccessible::nsHTMLTextAccessible(nsIPresShell* aShell, nsIDOMNode* aDomNode):
nsLeafDOMAccessible(aShell, aDomNode)
{ 
}

/* wstring getAccName (); */
NS_IMETHODIMP nsHTMLTextAccessible::GetAccName(PRUnichar **_retval)
{
  nsAutoString value;
  mNode->GetNodeValue(value);
  value.CompressWhitespace();
  *_retval = value.ToNewUnicode();
  return NS_OK;
}

/* wstring getAccRole (); */
NS_IMETHODIMP nsHTMLTextAccessible::GetAccRole(PRUnichar **_retval)
{
    nsAutoString role(NS_LITERAL_STRING("text"));
    *_retval = role.ToNewUnicode();

    return NS_OK;
}
