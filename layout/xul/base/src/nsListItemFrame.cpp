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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David W. Hyatt (hyatt@netscape.com) (Original Author)
 *   Joe Hewitt (hewitt@netscape.com)
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

#include "nsListItemFrame.h"

#include "nsCOMPtr.h"
#include "nsINameSpaceManager.h" 
#include "nsXULAtoms.h"

NS_IMETHODIMP_(nsrefcnt) 
nsListItemFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt)
nsListItemFrame::Release(void)
{
  return NS_OK;
}

NS_INTERFACE_MAP_BEGIN(nsListItemFrame)
NS_INTERFACE_MAP_END_INHERITING(nsGridRowLeafFrame)

nsListItemFrame::nsListItemFrame(nsIPresShell* aPresShell, PRBool aIsRoot, nsIBoxLayout* aLayoutManager)
  : nsGridRowLeafFrame(aPresShell, aIsRoot, aLayoutManager) 
{
}

nsListItemFrame::~nsListItemFrame()
{
}

nsresult
nsListItemFrame::GetPrefSize(nsBoxLayoutState& aState, nsSize& aSize)
{
  nsresult rv = nsBoxFrame::GetPrefSize(aState, aSize);
  if (NS_FAILED(rv)) return rv;

  // guarantee that our preferred height doesn't exceed the standard
  // listbox row height
  aSize.height = PR_MAX(mRect.height, aSize.height);
  return NS_OK;
}

NS_IMETHODIMP
nsListItemFrame::GetFrameForPoint(nsPresContext* aPresContext,
                                     const nsPoint& aPoint, 
                                     nsFramePaintLayer aWhichLayer,
                                     nsIFrame**     aFrame)
{
  nsAutoString value;
  mContent->GetAttr(kNameSpaceID_None, nsXULAtoms::allowevents, value);
  if (value.EqualsLiteral("true")) {
    return nsBoxFrame::GetFrameForPoint(aPresContext, aPoint, aWhichLayer, aFrame);
  }
  else if (mRect.Contains(aPoint)) {
    if (GetStyleVisibility()->IsVisible()) {
      *aFrame = this; // Capture all events so that we can perform selection and expand/collapse.
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

// Creation Routine ///////////////////////////////////////////////////////////////////////

nsresult
NS_NewListItemFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRBool aIsRoot, 
                        nsIBoxLayout* aLayoutManager)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsListItemFrame* it = new (aPresShell) nsListItemFrame(aPresShell, aIsRoot, aLayoutManager);
  if (!it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewListItemFrame

