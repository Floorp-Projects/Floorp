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
 * Original Author: John Gaunt (jgaunt@netscape.com)
 * Contributor(s):
 *   Aaron Leventhal (aaronl@netscape.com)
 *   Kyle Yuan (kyle.yuan@sun.com)
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

// NOTE: alphabetically ordered
#include "nsIDOMXULDescriptionElement.h"
#include "nsWeakReference.h"
#include "nsXULTextAccessible.h"

/**
  * For XUL descriptions and labels
  */
nsXULTextAccessible::nsXULTextAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell):
nsTextAccessible(aDomNode, aShell)
{ 
}

/* wstring getAccName (); */
NS_IMETHODIMP nsXULTextAccessible::GetAccName(nsAString& _retval)
{ 
  nsCOMPtr<nsIDOMXULDescriptionElement> descriptionElement(do_QueryInterface(mDOMNode));
  if (descriptionElement) {
    nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
    return AppendFlatStringFromSubtree(content, &_retval);
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsXULTextAccessible::GetAccState(PRUint32 *_retval)
{
  // Labels and description can only have read only state
  // They are not focusable or selectable
  *_retval = STATE_READONLY;
  return NS_OK;
}

/**
  * For XUL tooltip
  */
nsXULTooltipAccessible::nsXULTooltipAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell):
nsLeafAccessible(aDomNode, aShell)
{ 
}

NS_IMETHODIMP nsXULTooltipAccessible::GetAccName(nsAString& _retval)
{
  //XXX, kyle.yuan@sun.com, we don't know how to get at this information at the moment,
  //  because it is not loaded until it shows.
  return NS_OK;
}

NS_IMETHODIMP nsXULTooltipAccessible::GetAccState(PRUint32 *_retval)
{
  nsLeafAccessible::GetAccState(_retval);
  *_retval &= ~STATE_FOCUSABLE;
  *_retval |= STATE_READONLY;
  return NS_OK;
}

NS_IMETHODIMP nsXULTooltipAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_TOOLTIP;
  return NS_OK;
}
