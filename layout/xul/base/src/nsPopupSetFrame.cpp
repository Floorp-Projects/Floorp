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
 *   Mats Palmgren <matspal@gmail.com>
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
#include "nsBoxLayoutState.h"
#include "nsIScrollableFrame.h"
#include "nsIRootBox.h"
#include "nsMenuPopupFrame.h"

nsIFrame*
NS_NewPopupSetFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsPopupSetFrame (aPresShell, aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsPopupSetFrame)

NS_IMETHODIMP
nsPopupSetFrame::Init(nsIContent*      aContent,
                      nsIFrame*        aParent,
                      nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsBoxFrame::Init(aContent, aParent, aPrevInFlow);

  // Normally the root box is our grandparent, but in case of wrapping
  // it can be our great-grandparent.
  nsIRootBox *rootBox = nsIRootBox::GetRootBox(PresContext()->GetPresShell());
  if (rootBox) {
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
                              nsFrameList&    aFrameList)
{
  if (aListName == nsGkAtoms::popupList) {
    AddPopupFrameList(aFrameList);
    return NS_OK;
  }
  return nsBoxFrame::AppendFrames(aListName, aFrameList);
}

NS_IMETHODIMP
nsPopupSetFrame::RemoveFrame(nsIAtom*        aListName,
                             nsIFrame*       aOldFrame)
{
  if (aListName == nsGkAtoms::popupList) {
    RemovePopupFrame(aOldFrame);
    return NS_OK;
  }
  return nsBoxFrame::RemoveFrame(aListName, aOldFrame);
}

NS_IMETHODIMP
nsPopupSetFrame::InsertFrames(nsIAtom*        aListName,
                              nsIFrame*       aPrevFrame,
                              nsFrameList&    aFrameList)
{
  if (aListName == nsGkAtoms::popupList) {
    AddPopupFrameList(aFrameList);
    return NS_OK;
  }
  return nsBoxFrame::InsertFrames(aListName, aPrevFrame, aFrameList);
}

NS_IMETHODIMP
nsPopupSetFrame::SetInitialChildList(nsIAtom*        aListName,
                                     nsFrameList&    aChildList)
{
  if (aListName == nsGkAtoms::popupList) {
    // XXXmats this asserts because we don't implement
    // GetChildList(nsGkAtoms::popupList) so nsCSSFrameConstructor
    // believes it's empty and calls us multiple times.
    //NS_ASSERTION(mPopupList.IsEmpty(),
    //             "SetInitialChildList on non-empty child list");
    AddPopupFrameList(aChildList);
    return NS_OK;
  }
  return nsBoxFrame::SetInitialChildList(aListName, aChildList);
}

void
nsPopupSetFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  mPopupList.DestroyFramesFrom(aDestructRoot);

  // Normally the root box is our grandparent, but in case of wrapping
  // it can be our great-grandparent.
  nsIRootBox *rootBox = nsIRootBox::GetRootBox(PresContext()->GetPresShell());
  if (rootBox) {
    rootBox->SetPopupSetFrame(nsnull);
  }

  nsBoxFrame::DestroyFrom(aDestructRoot);
}

NS_IMETHODIMP
nsPopupSetFrame::DoLayout(nsBoxLayoutState& aState)
{
  // lay us out
  nsresult rv = nsBoxFrame::DoLayout(aState);

  // lay out all of our currently open popups.
  for (nsFrameList::Enumerator e(mPopupList); !e.AtEnd(); e.Next()) {
    nsMenuPopupFrame* popupChild = static_cast<nsMenuPopupFrame*>(e.get());
    popupChild->LayoutPopup(aState, nsnull, PR_FALSE);
  }

  return rv;
}

void
nsPopupSetFrame::RemovePopupFrame(nsIFrame* aPopup)
{
  NS_PRECONDITION((aPopup->GetStateBits() & NS_FRAME_OUT_OF_FLOW) &&
                  aPopup->GetType() == nsGkAtoms::menuPopupFrame,
                  "removing wrong type of frame in popupset's ::popupList");

  mPopupList.DestroyFrame(aPopup);
}

void
nsPopupSetFrame::AddPopupFrameList(nsFrameList& aPopupFrameList)
{
#ifdef DEBUG
  for (nsFrameList::Enumerator e(aPopupFrameList); !e.AtEnd(); e.Next()) {
    NS_ASSERTION((e.get()->GetStateBits() & NS_FRAME_OUT_OF_FLOW) &&
                 e.get()->GetType() == nsGkAtoms::menuPopupFrame,
                 "adding wrong type of frame in popupset's ::popupList");
  }
#endif
  mPopupList.InsertFrames(nsnull, nsnull, aPopupFrameList);
}

#ifdef DEBUG
NS_IMETHODIMP
nsPopupSetFrame::List(FILE* out, PRInt32 aIndent) const
{
  IndentBy(out, aIndent);
  ListTag(out);
#ifdef DEBUG_waterson
  fprintf(out, " [parent=%p]", static_cast<void*>(mParent));
#endif
  if (HasView()) {
    fprintf(out, " [view=%p]", static_cast<void*>(GetView()));
  }
  if (GetNextSibling()) {
    fprintf(out, " next=%p", static_cast<void*>(GetNextSibling()));
  }
  if (nsnull != GetPrevContinuation()) {
    fprintf(out, " prev-continuation=%p", static_cast<void*>(GetPrevContinuation()));
  }
  if (nsnull != GetNextContinuation()) {
    fprintf(out, " next-continuation=%p", static_cast<void*>(GetNextContinuation()));
  }
  fprintf(out, " {%d,%d,%d,%d}", mRect.x, mRect.y, mRect.width, mRect.height);
  if (0 != mState) {
    fprintf(out, " [state=%016llx]", mState);
  }
  fprintf(out, " [content=%p]", static_cast<void*>(mContent));
  nsPopupSetFrame* f = const_cast<nsPopupSetFrame*>(this);
  if (f->HasOverflowAreas()) {
    nsRect overflowArea = f->GetVisualOverflowRect();
    fprintf(out, " [vis-overflow=%d,%d,%d,%d]",
            overflowArea.x, overflowArea.y,
            overflowArea.width, overflowArea.height);
    overflowArea = f->GetScrollableOverflowRect();
    fprintf(out, " [scr-overflow=%d,%d,%d,%d]",
            overflowArea.x, overflowArea.y,
            overflowArea.width, overflowArea.height);
  }
  fprintf(out, " [sc=%p]", static_cast<void*>(mStyleContext));
  nsIAtom* pseudoTag = mStyleContext->GetPseudo();
  if (pseudoTag) {
    nsAutoString atomString;
    pseudoTag->ToString(atomString);
    fprintf(out, " pst=%s",
            NS_LossyConvertUTF16toASCII(atomString).get());
  }

  // Output the children
  PRBool outputOneList = PR_FALSE;
  ChildListIterator lists(this);
  for (; !lists.IsDone(); lists.Next()) {
    if (outputOneList) {
      IndentBy(out, aIndent);
    }
    outputOneList = PR_TRUE;
    fprintf(out, "%s<\n", mozilla::layout::ChildListName(lists.CurrentID()));
    nsFrameList::Enumerator childFrames(lists.CurrentList());
    for (; !childFrames.AtEnd(); childFrames.Next()) {
      nsIFrame* kid = childFrames.get();
      // Verify the child frame's parent frame pointer is correct
      NS_ASSERTION(kid->GetParent() == this, "bad parent frame pointer");

      // Have the child frame list
      kid->List(out, aIndent + 1);
    }
    IndentBy(out, aIndent);
    fputs(">\n", out);
  }

  // XXXmats the above is copy-pasted from nsContainerFrame::List which is lame,
  // clean this up after bug 399111 is implemented.

  if (!mPopupList.IsEmpty()) {
    fputs("<\n", out);
    ++aIndent;
    IndentBy(out, aIndent);
    fputs(mozilla::layout::ChildListName(kPopupList), out);
    fputs(" for ", out);
    ListTag(out);
    fputs(" <\n", out);
    ++aIndent;
    for (nsFrameList::Enumerator e(mPopupList); !e.AtEnd(); e.Next()) {
      e.get()->List(out, aIndent);
    }
    --aIndent;
    IndentBy(out, aIndent);
    fputs(">\n", out);
    --aIndent;
    IndentBy(out, aIndent);
    fputs(">\n", out);
    outputOneList = PR_TRUE;
  }

  if (!outputOneList) {
    fputs("<>\n", out);
  }

  return NS_OK;
}
#endif
