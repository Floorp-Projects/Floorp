/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Author: Aaron Leventhal (aaronl@netscape.com)
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

