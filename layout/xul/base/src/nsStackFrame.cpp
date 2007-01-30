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

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsStackFrame.h"
#include "nsStyleContext.h"
#include "nsPresContext.h"
#include "nsIContent.h"
#include "nsCOMPtr.h"
#include "nsUnitConversion.h"
#include "nsHTMLParts.h"
#include "nsIPresShell.h"
#include "nsCSSRendering.h"
#include "nsIViewManager.h"
#include "nsBoxLayoutState.h"
#include "nsStackLayout.h"
#include "nsDisplayList.h"

nsIFrame*
NS_NewStackFrame (nsIPresShell* aPresShell, nsStyleContext* aContext, nsIBoxLayout* aLayoutManager)
{
  return new (aPresShell) nsStackFrame(aPresShell, aContext, aLayoutManager);
} // NS_NewStackFrame

nsStackFrame::nsStackFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, nsIBoxLayout* aLayoutManager):
  nsBoxFrame(aPresShell, aContext)
{
    // if no layout manager specified us the stack layout
  nsCOMPtr<nsIBoxLayout> layout = aLayoutManager;

  if (layout == nsnull) {
    NS_NewStackLayout(aPresShell, layout);
  }

  SetLayoutManager(layout);
}

// REVIEW: The old code put everything in the background layer. To be more
// consistent with the way other frames work, I'm putting everything in the
// Content() (i.e., foreground) layer (see nsFrame::BuildDisplayListForChild,
// the case for stacking context but non-positioned, non-floating frames).
// This could easily be changed back by hacking nsBoxFrame::BuildDisplayListInternal
// a bit more.
NS_IMETHODIMP
nsStackFrame::BuildDisplayListForChildren(nsDisplayListBuilder*   aBuilder,
                                          const nsRect&           aDirtyRect,
                                          const nsDisplayListSet& aLists)
{
  // BuildDisplayListForChild puts stacking contexts into the PositionedDescendants
  // list. So we need to map that list to aLists.Content(). This is an easy way to
  // do that.
  nsDisplayList* content = aLists.Content();
  nsDisplayListSet kidLists(content, content, content, content, content, content);
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    // Force each child into its own true stacking context.
    nsresult rv =
      BuildDisplayListForChild(aBuilder, kid, aDirtyRect, kidLists,
                               DISPLAY_CHILD_FORCE_STACKING_CONTEXT);
    NS_ENSURE_SUCCESS(rv, rv);
    kid = kid->GetNextSibling();
  }
  return NS_OK;
}
