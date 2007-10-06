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
 *   Original Author: David W. Hyatt (hyatt@netscape.com)
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Dean Tessman <dean_tessman@hotmail.com>
 *   Mats Palmgren <mats.palmgren@bredband.net>
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

#include "nsPopupSetFrame.h"
#include "nsGkAtoms.h"
#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "nsCSSRendering.h"
#include "nsINameSpaceManager.h"
#include "nsIDocument.h"
#include "nsBoxLayoutState.h"
#include "nsIScrollableFrame.h"
#include "nsCSSFrameConstructor.h"
#include "nsGUIEvent.h"
#include "nsIRootBox.h"

nsPopupFrameList::nsPopupFrameList(nsIContent* aPopupContent, nsPopupFrameList* aNext)
:mNextPopup(aNext), 
 mPopupFrame(nsnull),
 mPopupContent(aPopupContent)
{
}

//
// NS_NewPopupSetFrame
//
// Wrapper for creating a new menu popup container
//
nsIFrame*
NS_NewPopupSetFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsPopupSetFrame (aPresShell, aContext);
}

NS_IMETHODIMP
nsPopupSetFrame::Init(nsIContent*      aContent,
                      nsIFrame*        aParent,
                      nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsBoxFrame::Init(aContent, aParent, aPrevInFlow);

  nsIRootBox *rootBox;
  nsresult res = CallQueryInterface(aParent->GetParent(), &rootBox);
  NS_ASSERTION(NS_SUCCEEDED(res), "grandparent should be root box");
  if (NS_SUCCEEDED(res)) {
    rootBox->SetPopupSetFrame(this);
  }

  return rv;
}

nsIAtom*
nsPopupSetFrame::GetType() const
{
  return nsGkAtoms::popupSetFrame;
}

NS_IMETHODIMP
nsPopupSetFrame::AppendFrames(nsIAtom*        aListName,
                              nsIFrame*       aFrameList)
{
  if (aListName == nsGkAtoms::popupList) {
    return AddPopupFrameList(aFrameList);
  }
  return nsBoxFrame::AppendFrames(aListName, aFrameList);
}

NS_IMETHODIMP
nsPopupSetFrame::RemoveFrame(nsIAtom*        aListName,
                             nsIFrame*       aOldFrame)
{
  if (aListName == nsGkAtoms::popupList) {
    return RemovePopupFrame(aOldFrame);
  }
  return nsBoxFrame::RemoveFrame(aListName, aOldFrame);
}

NS_IMETHODIMP
nsPopupSetFrame::InsertFrames(nsIAtom*        aListName,
                              nsIFrame*       aPrevFrame,
                              nsIFrame*       aFrameList)
{
  if (aListName == nsGkAtoms::popupList) {
    return AddPopupFrameList(aFrameList);
  }
  return nsBoxFrame::InsertFrames(aListName, aPrevFrame, aFrameList);
}

NS_IMETHODIMP
nsPopupSetFrame::SetInitialChildList(nsIAtom*        aListName,
                                     nsIFrame*       aChildList)
{
  if (aListName == nsGkAtoms::popupList) {
    return AddPopupFrameList(aChildList);
  }
  return nsBoxFrame::SetInitialChildList(aListName, aChildList);
}

void
nsPopupSetFrame::Destroy()
{
  nsCSSFrameConstructor* frameConstructor =
    PresContext()->PresShell()->FrameConstructor();
  // remove each popup from the list as we go.
  while (mPopupList) {
    if (mPopupList->mPopupFrame)
      DestroyPopupFrame(frameConstructor, mPopupList->mPopupFrame);

    nsPopupFrameList* temp = mPopupList;
    mPopupList = mPopupList->mNextPopup;
    delete temp;
  }

  nsIRootBox *rootBox;
  nsresult res = CallQueryInterface(mParent->GetParent(), &rootBox);
  NS_ASSERTION(NS_SUCCEEDED(res), "grandparent should be root box");
  if (NS_SUCCEEDED(res)) {
    rootBox->SetPopupSetFrame(nsnull);
  }

  nsBoxFrame::Destroy();
}

NS_IMETHODIMP
nsPopupSetFrame::DoLayout(nsBoxLayoutState& aState)
{
  // lay us out
  nsresult rv = nsBoxFrame::DoLayout(aState);

  // lay out all of our currently open popups.
  nsPopupFrameList* currEntry = mPopupList;
  while (currEntry) {
    nsMenuPopupFrame* popupChild = currEntry->mPopupFrame;
    if (popupChild && popupChild->IsOpen()) {
      // then get its preferred size
      nsSize prefSize = popupChild->GetPrefSize(aState);
      nsSize minSize = popupChild->GetMinSize(aState);
      nsSize maxSize = popupChild->GetMaxSize(aState);

      BoundsCheck(minSize, prefSize, maxSize);

      popupChild->SetBounds(aState, nsRect(0,0,prefSize.width, prefSize.height));
      popupChild->SetPopupPosition(nsnull);

      // is the new size too small? Make sure we handle scrollbars correctly
      nsIBox* child = popupChild->GetChildBox();

      nsRect bounds(popupChild->GetRect());

      nsCOMPtr<nsIScrollableFrame> scrollframe = do_QueryInterface(child);
      if (scrollframe &&
          scrollframe->GetScrollbarStyles().mVertical == NS_STYLE_OVERFLOW_AUTO) {
        // if our pref height
        if (bounds.height < prefSize.height) {
          // layout the child
          popupChild->Layout(aState);

          nsMargin scrollbars = scrollframe->GetActualScrollbarSizes();
          if (bounds.width < prefSize.width + scrollbars.left + scrollbars.right)
          {
            bounds.width += scrollbars.left + scrollbars.right;
            popupChild->SetBounds(aState, bounds);
          }
        }
      }

      // layout the child
      popupChild->Layout(aState);
      // if the width or height changed, readjust the popup position. This is a
      // special case for tooltips where the preferred height doesn't include the
      // real height for its inline element, but does once it is laid out.
      // This is bug 228673 which doesn't have a simple fix.
      if (popupChild->GetRect().width > bounds.width ||
          popupChild->GetRect().height > bounds.height)
        popupChild->SetPopupPosition(nsnull);
      popupChild->AdjustView();
    }

    currEntry = currEntry->mNextPopup;
  }

  return rv;
}

nsresult
nsPopupSetFrame::RemovePopupFrame(nsIFrame* aPopup)
{
  // This was called by the Destroy() method of the popup, so all we have to do is
  // get the popup out of our list, so we don't reflow it later.
  nsPopupFrameList* currEntry = mPopupList;
  nsPopupFrameList* temp = nsnull;
  while (currEntry) {
    if (currEntry->mPopupFrame == aPopup) {
      // Remove this entry.
      if (temp)
        temp->mNextPopup = currEntry->mNextPopup;
      else
        mPopupList = currEntry->mNextPopup;
      
      // Destroy the frame.
      DestroyPopupFrame(PresContext()->PresShell()->FrameConstructor(),
                        currEntry->mPopupFrame);

      // Delete the entry.
      currEntry->mNextPopup = nsnull;
      delete currEntry;

      // Break out of the loop.
      break;
    }

    temp = currEntry;
    currEntry = currEntry->mNextPopup;
  }

  return NS_OK;
}

nsresult
nsPopupSetFrame::AddPopupFrameList(nsIFrame* aPopupFrameList)
{
  for (nsIFrame* kid = aPopupFrameList; kid; kid = kid->GetNextSibling()) {
    nsresult rv = AddPopupFrame(kid);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

nsresult
nsPopupSetFrame::AddPopupFrame(nsIFrame* aPopup)
{
  NS_ASSERTION(aPopup->GetType() == nsGkAtoms::menuPopupFrame,
               "expected a menupopup frame to be added to a popupset");
  if (aPopup->GetType() != nsGkAtoms::menuPopupFrame)
    return NS_ERROR_UNEXPECTED;

  // The entry should already exist, but might not (if someone decided to make their
  // popup visible straightaway, e.g., the autocomplete widget).
  // First look for an entry by content.
  nsIContent* content = aPopup->GetContent();
  nsPopupFrameList* entry = mPopupList;
  while (entry && entry->mPopupContent != content)
    entry = entry->mNextPopup;
  if (!entry) {
    entry = new nsPopupFrameList(content, mPopupList);
    if (!entry)
      return NS_ERROR_OUT_OF_MEMORY;
    mPopupList = entry;
  }
  else {
    NS_ASSERTION(!entry->mPopupFrame, "Leaking a popup frame");
  }

  // Set the frame connection.
  entry->mPopupFrame = static_cast<nsMenuPopupFrame *>(aPopup);
  
  return NS_OK;
}

void
nsPopupSetFrame::DestroyPopupFrame(nsCSSFrameConstructor* aFc, nsIFrame* aPopup)
{
  aFc->RemoveMappingsForFrameSubtree(aPopup);
  aPopup->Destroy();
}
