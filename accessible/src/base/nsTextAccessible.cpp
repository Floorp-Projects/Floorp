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
 *   John Gaunt (jgaunt@netscape.com)
 *   Aaron Leventhal (aaronl@netscape.com)
 *   Kyle Yuan (kyle.yuan@sun.com)
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

// NOTE: alphabetically ordered
#include "nsTextAccessible.h"

// ------------
// Text Accessibles
// ------------

nsTextAccessible::nsTextAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell):
nsLinkableAccessible(aDOMNode, aShell)
{ 
}

NS_IMPL_ISUPPORTS_INHERITED0(nsTextAccessible, nsLinkableAccessible)

/**
  * We are text
  */
NS_IMETHODIMP nsTextAccessible::GetRole(PRUint32 *_retval)
{
  *_retval = nsIAccessibleRole::ROLE_TEXT_LEAF;
  return NS_OK;
}

/**
  * No Children
  */
NS_IMETHODIMP nsTextAccessible::GetFirstChild(nsIAccessible **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

/**
  * No Children
  */
NS_IMETHODIMP nsTextAccessible::GetLastChild(nsIAccessible **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

/**
  * No Children
  */
NS_IMETHODIMP nsTextAccessible::GetChildCount(PRInt32 *_retval)
{
  *_retval = 0;
  return NS_OK;
}

