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
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Original Author: Aaron Leventhal (aaronl@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsOuterDocAccessible.h"

NS_IMPL_ISUPPORTS_INHERITED0(nsOuterDocAccessible, nsBlockAccessible)

nsOuterDocAccessible::nsOuterDocAccessible(nsIDOMNode* aNode, 
                                           nsIAccessible* aDocAccessible, 
                                           nsIWeakReference* aShell):
  nsBlockAccessible(aNode, aShell), mInnerDocAccessible(aDocAccessible)
{
}

  /* attribute wstring accName; */
NS_IMETHODIMP nsOuterDocAccessible::GetAccName(nsAString& aAccName) 
{ 
  nsCOMPtr<nsIAccessibleDocument> accDoc(do_QueryInterface(mInnerDocAccessible));
  nsresult rv = accDoc->GetTitle(aAccName);
  if (NS_FAILED(rv) || aAccName.IsEmpty())
    rv = accDoc->GetURL(aAccName);
  return rv;
}

NS_IMETHODIMP nsOuterDocAccessible::GetAccValue(nsAString& aAccValue) 
{ 
  return NS_OK;
}

/* nsIAccessible getAccFirstChild (); */
NS_IMETHODIMP nsOuterDocAccessible::GetAccFirstChild(nsIAccessible **aChild)
{
  NS_IF_ADDREF(*aChild = mInnerDocAccessible);
  return NS_OK;
}

/* nsIAccessible getAccLastChild (); */
NS_IMETHODIMP nsOuterDocAccessible::GetAccLastChild(nsIAccessible **aChild)
{
  NS_IF_ADDREF(*aChild = mInnerDocAccessible);
  return NS_OK;
}

/* long getAccChildCount (); */
NS_IMETHODIMP nsOuterDocAccessible::GetAccChildCount(PRInt32 *aNumChildren)
{
  *aNumChildren = 1;
  return NS_OK;
}

/* unsigned long getAccRole (); */
NS_IMETHODIMP nsOuterDocAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_CLIENT;
  return NS_OK;
}

NS_IMETHODIMP nsOuterDocAccessible::GetAccState(PRUint32 *aAccState)
{
  return nsAccessible::GetAccState(aAccState);
}
