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
 *   Author: Aaron Leventhal (aaronl@netscape.com)
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

#include "nsHTMLLinkAccessible.h"
#include "nsAccessibilityAtoms.h"
#include "nsIAccessibleEvent.h"
#include "nsINameSpaceManager.h"

NS_IMPL_ISUPPORTS_INHERITED0(nsHTMLLinkAccessible, nsLinkableAccessible)

nsHTMLLinkAccessible::nsHTMLLinkAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell, nsIFrame *aFrame):
nsLinkableAccessible(aDomNode, aShell), mFrame(aFrame)
{ 
}

/* wstring getName (); */
NS_IMETHODIMP nsHTMLLinkAccessible::GetName(nsAString& aName)
{ 
  if (!mActionContent)
    return NS_ERROR_FAILURE;

  return AppendFlatStringFromSubtree(mActionContent, &aName);
}

/* unsigned long getRole (); */
NS_IMETHODIMP nsHTMLLinkAccessible::GetRole(PRUint32 *_retval)
{
  *_retval = nsIAccessibleRole::ROLE_LINK;

  return NS_OK;
}

NS_IMETHODIMP nsHTMLLinkAccessible::GetState(PRUint32 *aState)
{
  nsLinkableAccessible::GetState(aState);
  *aState  &= ~STATE_READONLY;

  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  if (content && content->HasAttr(kNameSpaceID_None,
                                  nsAccessibilityAtoms::name)) {
    // This is how we indicate it is a named anchor
    // In other words, this anchor can be selected as a location :)
    // There is no other better state to use to indicate this.
    *aState |= STATE_SELECTABLE;
  }

  return NS_OK;
}

nsIFrame* nsHTMLLinkAccessible::GetFrame()
{
  if (mWeakShell) {
    if (!mFrame) {
      mFrame = nsLinkableAccessible::GetFrame();
    }
    return mFrame;
  }
  return nsnull;
}

NS_IMETHODIMP nsHTMLLinkAccessible::FireToolkitEvent(PRUint32 aEvent,
                                                     nsIAccessible *aTarget,
                                                     void *aData)
{
  if (aEvent == nsIAccessibleEvent::EVENT_HIDE) {
    mFrame = nsnull;  // Invalidate cached frame
  }
  return nsLinkableAccessible::FireToolkitEvent(aEvent, aTarget, aData);
}
