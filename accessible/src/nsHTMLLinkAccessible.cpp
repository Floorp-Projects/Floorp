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
 * Author: Aaron Leventhal (aaronl@netscape.com)
 * Contributor(s): 
 */

#include "nsHTMLLinkAccessible.h"
#include "nsWeakReference.h"
#include "nsIFrame.h"
#include "nsILink.h"
#include "nsILinkHandler.h"
#include "nsISelection.h"
#include "nsISelectionController.h"
#include "nsIPresContext.h"
#include "nsReadableUtils.h"
#include "nsIDOMElement.h"

nsHTMLLinkAccessible::nsHTMLLinkAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell):
nsLinkableAccessible(aDomNode, aShell)
{ 
}

/* wstring getAccName (); */
NS_IMETHODIMP nsHTMLLinkAccessible::GetAccName(nsAWritableString& _retval)
{ 
  if (!IsALink())  // Also initializes private data members
    return NS_ERROR_FAILURE;

  nsAutoString name;
  nsresult rv = AppendFlatStringFromSubtree(mLinkContent, &name);
  if (NS_SUCCEEDED(rv)) {
    // Temp var needed until CompressWhitespace built for nsAWritableString
    name.CompressWhitespace();
    _retval.Assign(name);
  }
  return rv;
}

/* unsigned long getAccRole (); */
NS_IMETHODIMP nsHTMLLinkAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_LINK;

  return NS_OK;
}

