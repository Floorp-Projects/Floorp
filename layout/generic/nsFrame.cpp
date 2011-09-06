/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Uri Bernstein <uriber@gmail.com>
 *   Eli Friedman <sharparrow1@yahoo.com>
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

/* base class of all rendering objects */

#include "nsCOMPtr.h"
#include "nsFrame.h"
#include "nsFrameList.h"
#include "nsPlaceholderFrame.h"
#include "nsLineLayout.h"
#include "nsIContent.h"
#include "nsContentUtils.h"
#include "nsIAtom.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsStyleContext.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsIScrollableFrame.h"
#include "nsPresContext.h"
#include "nsCRT.h"
#include "nsGUIEvent.h"
#include "nsIDOMEvent.h"
#include "nsPLDOMEvent.h"
#include "nsStyleConsts.h"
#include "nsIPresShell.h"
#include "prlog.h"
#include "prprf.h"
#include <stdarg.h>
#include "nsFrameManager.h"
#include "nsCSSRendering.h"
#include "nsLayoutUtils.h"
#ifdef ACCESSIBILITY
#include "nsIAccessible.h"
#endif

#include "nsIDOMNode.h"
#include "nsIEditorDocShell.h"
#include "nsEventStateManager.h"
#include "nsISelection.h"
#include "nsISelectionPrivate.h"
#include "nsFrameSelection.h"
#include "nsHTMLParts.h"
#include "nsGkAtoms.h"
#include "nsCSSAnonBoxes.h"
#include "nsCSSPseudoElements.h"
#include "nsIHTMLContentSink.h" 
#include "nsCSSFrameConstructor.h"

#include "nsFrameTraversal.h"
#include "nsStyleChangeList.h"
#include "nsIDOMRange.h"
#include "nsITableLayout.h"    //selection necessity
#include "nsITableCellLayout.h"//  "
#include "nsITextControlFrame.h"
#include "nsINameSpaceManager.h"
#include "nsIPercentHeightObserver.h"
#include "nsStyleStructInlines.h"

#ifdef IBMBIDI
#include "nsBidiPresUtils.h"
#endif

// For triple-click pref
#include "nsIServiceManager.h"
#include "imgIContainer.h"
#include "imgIRequest.h"
#include "nsILookAndFeel.h"
#include "nsLayoutCID.h"
#include "nsWidgetsCID.h"     // for NS_LOOKANDFEEL_CID
#include "nsUnicharUtils.h"
#include "nsLayoutErrors.h"
#include "nsContentErrors.h"
#include "nsHTMLContainerFrame.h"
#include "nsBoxLayoutState.h"
#include "nsBlockFrame.h"
#include "nsDisplayList.h"
#include "nsIObjectLoadingContent.h"
#include "nsExpirationTracker.h"
#include "nsSVGIntegrationUtils.h"
#include "nsSVGEffects.h"
#include "nsChangeHint.h"

#include "gfxContext.h"
#include "CSSCalc.h"

#include "mozilla/Preferences.h"

using namespace mozilla;
using namespace mozilla::layers;

static NS_DEFINE_CID(kLookAndFeelCID,  NS_LOOKANDFEEL_CID);

// Struct containing cached metrics for box-wrapped frames.
struct nsBoxLayoutMetrics
{
  nsSize mPrefSize;
  nsSize mMinSize;
  nsSize mMaxSize;

  nsSize mBlockMinSize;
  nsSize mBlockPrefSize;
  nscoord mBlockAscent;

  nscoord mFlex;
  nscoord mAscent;

  nsSize mLastSize;
};

struct nsContentAndOffset
{
  nsIContent* mContent;
  PRInt32 mOffset;
};

// Some Misc #defines
#define SELECTION_DEBUG        0
#define FORCE_SELECTION_UPDATE 1
#define CALC_DEBUG             0


#include "nsILineIterator.h"

//non Hack prototypes
#if 0
static void RefreshContentFrames(nsPresContext* aPresContext, nsIContent * aStartContent, nsIContent * aEndContent);
#endif

#include "prenv.h"

// Formerly the nsIFrameDebug interface

#ifdef NS_DEBUG
static PRBool gShowFrameBorders = PR_FALSE;

void nsFrame::ShowFrameBorders(PRBool aEnable)
{
  gShowFrameBorders = aEnable;
}

PRBool nsFrame::GetShowFrameBorders()
{
  return gShowFrameBorders;
}

static PRBool gShowEventTargetFrameBorder = PR_FALSE;

void nsFrame::ShowEventTargetFrameBorder(PRBool aEnable)
{
  gShowEventTargetFrameBorder = aEnable;
}

PRBool nsFrame::GetShowEventTargetFrameBorder()
{
  return gShowEventTargetFrameBorder;
}

/**
 * Note: the log module is created during library initialization which
 * means that you cannot perform logging before then.
 */
static PRLogModuleInfo* gLogModule;

static PRLogModuleInfo* gStyleVerifyTreeLogModuleInfo;

static PRUint32 gStyleVerifyTreeEnable = 0x55;

PRBool
nsFrame::GetVerifyStyleTreeEnable()
{
  if (gStyleVerifyTreeEnable == 0x55) {
    if (nsnull == gStyleVerifyTreeLogModuleInfo) {
      gStyleVerifyTreeLogModuleInfo = PR_NewLogModule("styleverifytree");
      gStyleVerifyTreeEnable = 0 != gStyleVerifyTreeLogModuleInfo->level;
    }
  }
  return gStyleVerifyTreeEnable;
}

void
nsFrame::SetVerifyStyleTreeEnable(PRBool aEnabled)
{
  gStyleVerifyTreeEnable = aEnabled;
}

PRLogModuleInfo*
nsFrame::GetLogModuleInfo()
{
  if (nsnull == gLogModule) {
    gLogModule = PR_NewLogModule("frame");
  }
  return gLogModule;
}

void
nsFrame::DumpFrameTree(nsIFrame* aFrame)
{
    RootFrameList(aFrame->PresContext(), stdout, 0);
}

void
nsFrame::RootFrameList(nsPresContext* aPresContext, FILE* out, PRInt32 aIndent)
{
  if (!aPresContext || !out)
    return;

  nsIPresShell *shell = aPresContext->GetPresShell();
  if (shell) {
    nsIFrame* frame = shell->FrameManager()->GetRootFrame();
    if(frame) {
      frame->List(out, aIndent);
    }
  }
}
#endif

static PRBool ApplyOverflowClipping(nsDisplayListBuilder* aBuilder,
                                    const nsIFrame* aFrame,
                                    const nsStyleDisplay* aDisp, 
                                    nsRect* aRect);

static PRBool ApplyAbsPosClipping(nsDisplayListBuilder* aBuilder,
                                  const nsStyleDisplay* aDisp, 
                                  const nsIFrame* aFrame,
                                  nsRect* aRect);

void
NS_MergeReflowStatusInto(nsReflowStatus* aPrimary, nsReflowStatus aSecondary)
{
  *aPrimary |= aSecondary &
    (NS_FRAME_NOT_COMPLETE | NS_FRAME_OVERFLOW_INCOMPLETE |
     NS_FRAME_TRUNCATED | NS_FRAME_REFLOW_NEXTINFLOW);
  if (*aPrimary & NS_FRAME_NOT_COMPLETE) {
    *aPrimary &= ~NS_FRAME_OVERFLOW_INCOMPLETE;
  }
}

void
nsWeakFrame::InitInternal(nsIFrame* aFrame)
{
  Clear(mFrame ? mFrame->PresContext()->GetPresShell() : nsnull);
  mFrame = aFrame;
  if (mFrame) {
    nsIPresShell* shell = mFrame->PresContext()->GetPresShell();
    NS_WARN_IF_FALSE(shell, "Null PresShell in nsWeakFrame!");
    if (shell) {
      shell->AddWeakFrame(this);
    } else {
      mFrame = nsnull;
    }
  }
}

nsIFrame*
NS_NewEmptyFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsFrame(aContext);
}

nsFrame::nsFrame(nsStyleContext* aContext)
{
  MOZ_COUNT_CTOR(nsFrame);

  mState = NS_FRAME_FIRST_REFLOW | NS_FRAME_IS_DIRTY;
  mStyleContext = aContext;
  mStyleContext->AddRef();
}

nsFrame::~nsFrame()
{
  MOZ_COUNT_DTOR(nsFrame);

  NS_IF_RELEASE(mContent);
  if (mStyleContext)
    mStyleContext->Release();
}

NS_IMPL_FRAMEARENA_HELPERS(nsFrame)

// Dummy operator delete.  Will never be called, but must be defined
// to satisfy some C++ ABIs.
void
nsFrame::operator delete(void *, size_t)
{
  NS_RUNTIMEABORT("nsFrame::operator delete should never be called");
}

NS_QUERYFRAME_HEAD(nsFrame)
  NS_QUERYFRAME_ENTRY(nsIFrame)
NS_QUERYFRAME_TAIL_INHERITANCE_ROOT

/////////////////////////////////////////////////////////////////////////////
// nsIFrame

NS_IMETHODIMP
nsFrame::Init(nsIContent*      aContent,
              nsIFrame*        aParent,
              nsIFrame*        aPrevInFlow)
{
  NS_PRECONDITION(!mContent, "Double-initing a frame?");
  NS_ASSERTION(IsFrameOfType(eDEBUGAllFrames) &&
               !IsFrameOfType(eDEBUGNoFrames),
               "IsFrameOfType implementation that doesn't call base class");

  mContent = aContent;
  mParent = aParent;

  if (aContent) {
    NS_ADDREF(aContent);
  }

  if (aPrevInFlow) {
    // Make sure the general flags bits are the same
    nsFrameState state = aPrevInFlow->GetStateBits();

    // Make bits that are currently off (see constructor) the same:
    mState |= state & (NS_FRAME_SELECTED_CONTENT |
                       NS_FRAME_INDEPENDENT_SELECTION |
                       NS_FRAME_IS_SPECIAL |
                       NS_FRAME_MAY_BE_TRANSFORMED);
  }
  if (mParent) {
    nsFrameState state = mParent->GetStateBits();

    // Make bits that are currently off (see constructor) the same:
    mState |= state & (NS_FRAME_INDEPENDENT_SELECTION |
                       NS_FRAME_GENERATED_CONTENT);
  }
  if (GetStyleDisplay()->HasTransform()) {
    // The frame gets reconstructed if we toggle the -moz-transform
    // property, so we can set this bit here and then ignore it.
    mState |= NS_FRAME_MAY_BE_TRANSFORMED;
  }
  
  DidSetStyleContext(nsnull);

  if (IsBoxWrapped())
    InitBoxMetrics(PR_FALSE);

  return NS_OK;
}

NS_IMETHODIMP nsFrame::SetInitialChildList(ChildListID     aListID,
                                           nsFrameList&    aChildList)
{
  // XXX This shouldn't be getting called at all, but currently is for backwards
  // compatility reasons...
#if 0
  NS_ERROR("not a container");
  return NS_ERROR_UNEXPECTED;
#else
  NS_ASSERTION(aChildList.IsEmpty(), "not a container");
  return NS_OK;
#endif
}

NS_IMETHODIMP
nsFrame::AppendFrames(ChildListID     aListID,
                      nsFrameList&    aFrameList)
{
  NS_PRECONDITION(PR_FALSE, "not a container");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsFrame::InsertFrames(ChildListID     aListID,
                      nsIFrame*       aPrevFrame,
                      nsFrameList&    aFrameList)
{
  NS_PRECONDITION(PR_FALSE, "not a container");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsFrame::RemoveFrame(ChildListID     aListID,
                     nsIFrame*       aOldFrame)
{
  NS_PRECONDITION(PR_FALSE, "not a container");
  return NS_ERROR_UNEXPECTED;
}

void
nsFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  NS_ASSERTION(!nsContentUtils::IsSafeToRunScript(),
    "destroy called on frame while scripts not blocked");
  NS_ASSERTION(!GetNextSibling() && !GetPrevSibling(),
               "Frames should be removed before destruction.");
  NS_ASSERTION(aDestructRoot, "Must specify destruct root");

  nsSVGEffects::InvalidateDirectRenderingObservers(this);

  // Get the view pointer now before the frame properties disappear
  // when we call NotifyDestroyingFrame()
  nsIView* view = GetView();
  nsPresContext* presContext = PresContext();

  nsIPresShell *shell = presContext->GetPresShell();
  if (mState & NS_FRAME_OUT_OF_FLOW) {
    nsPlaceholderFrame* placeholder =
      shell->FrameManager()->GetPlaceholderFrameFor(this);
    NS_ASSERTION(!placeholder || (aDestructRoot != this),
                 "Don't call Destroy() on OOFs, call Destroy() on the placeholder.");
    NS_ASSERTION(!placeholder ||
                 nsLayoutUtils::IsProperAncestorFrame(aDestructRoot, placeholder),
                 "Placeholder relationship should have been torn down already; "
                 "this might mean we have a stray placeholder in the tree.");
    if (placeholder) {
      shell->FrameManager()->UnregisterPlaceholderFrame(placeholder);
      placeholder->SetOutOfFlowFrame(nsnull);
    }
  }

  // If we have any IB split special siblings, clear their references to us.
  // (Note: This has to happen before we call shell->NotifyDestroyingFrame,
  // because that clears our Properties() table.)
  if (mState & NS_FRAME_IS_SPECIAL) {
    // Delete previous sibling's reference to me.
    nsIFrame* prevSib = static_cast<nsIFrame*>
      (Properties().Get(nsIFrame::IBSplitSpecialPrevSibling()));
    if (prevSib) {
      NS_WARN_IF_FALSE(this ==
         prevSib->Properties().Get(nsIFrame::IBSplitSpecialSibling()),
         "IB sibling chain is inconsistent");
      prevSib->Properties().Delete(nsIFrame::IBSplitSpecialSibling());
    }

    // Delete next sibling's reference to me.
    nsIFrame* nextSib = static_cast<nsIFrame*>
      (Properties().Get(nsIFrame::IBSplitSpecialSibling()));
    if (nextSib) {
      NS_WARN_IF_FALSE(this ==
         nextSib->Properties().Get(nsIFrame::IBSplitSpecialPrevSibling()),
         "IB sibling chain is inconsistent");
      nextSib->Properties().Delete(nsIFrame::IBSplitSpecialPrevSibling());
    }
  }

  shell->NotifyDestroyingFrame(this);

  if ((mState & NS_FRAME_EXTERNAL_REFERENCE) ||
      (mState & NS_FRAME_SELECTED_CONTENT)) {
    shell->ClearFrameRefs(this);
  }

  if (view) {
    // Break association between view and frame
    view->SetClientData(nsnull);

    // Destroy the view
    view->Destroy();
  }

  // Make sure that our deleted frame can't be returned from GetPrimaryFrame()
  if (mContent && mContent->GetPrimaryFrame() == this) {
    mContent->SetPrimaryFrame(nsnull);
  }

  // Must retrieve the object ID before calling destructors, so the
  // vtable is still valid.
  //
  // Note to future tweakers: having the method that returns the
  // object size call the destructor will not avoid an indirect call;
  // the compiler cannot devirtualize the call to the destructor even
  // if it's from a method defined in the same class.

  nsQueryFrame::FrameIID id = GetFrameId();
  this->~nsFrame();

  // Now that we're totally cleaned out, we need to add ourselves to
  // the presshell's recycler.
  shell->FreeFrame(id, this);
}

NS_IMETHODIMP
nsFrame::GetOffsets(PRInt32 &aStart, PRInt32 &aEnd) const
{
  aStart = 0;
  aEnd = 0;
  return NS_OK;
}

static PRBool
EqualImages(imgIRequest *aOldImage, imgIRequest *aNewImage)
{
  if (aOldImage == aNewImage)
    return PR_TRUE;

  if (!aOldImage || !aNewImage)
    return PR_FALSE;

  nsCOMPtr<nsIURI> oldURI, newURI;
  aOldImage->GetURI(getter_AddRefs(oldURI));
  aNewImage->GetURI(getter_AddRefs(newURI));
  PRBool equal;
  return NS_SUCCEEDED(oldURI->Equals(newURI, &equal)) && equal;
}

// Subclass hook for style post processing
/* virtual */ void
nsFrame::DidSetStyleContext(nsStyleContext* aOldStyleContext)
{
  if (aOldStyleContext) {
    // If the old context had a background image image and new context
    // does not have the same image, clear the image load notifier
    // (which keeps the image loading, if it still is) for the frame.
    // We want to do this conservatively because some frames paint their
    // backgrounds from some other frame's style data, and we don't want
    // to clear those notifiers unless we have to.  (They'll be reset
    // when we paint, although we could miss a notification in that
    // interval.)
    const nsStyleBackground *oldBG = aOldStyleContext->GetStyleBackground();
    const nsStyleBackground *newBG = GetStyleBackground();
    NS_FOR_VISIBLE_BACKGROUND_LAYERS_BACK_TO_FRONT(i, oldBG) {
      if (i >= newBG->mImageCount ||
          oldBG->mLayers[i].mImage != newBG->mLayers[i].mImage) {
        // stop the image loading for the frame, the image has changed
        PresContext()->SetImageLoaders(this,
          nsPresContext::BACKGROUND_IMAGE, nsnull);
        break;
      }
    }

    // If we detect a change on margin, padding or border, we store the old
    // values on the frame itself between now and reflow, so if someone
    // calls GetUsed(Margin|Border|Padding)() before the next reflow, we
    // can give an accurate answer.
    // We don't want to set the property if one already exists.
    FrameProperties props = Properties();
    nsMargin oldValue(0, 0, 0, 0);
    nsMargin newValue(0, 0, 0, 0);
    const nsStyleMargin* oldMargin = aOldStyleContext->PeekStyleMargin();
    if (oldMargin && oldMargin->GetMargin(oldValue)) {
      if ((!GetStyleMargin()->GetMargin(newValue) || oldValue != newValue) &&
          !props.Get(UsedMarginProperty())) {
        props.Set(UsedMarginProperty(), new nsMargin(oldValue));
      }
    }

    const nsStylePadding* oldPadding = aOldStyleContext->PeekStylePadding();
    if (oldPadding && oldPadding->GetPadding(oldValue)) {
      if ((!GetStylePadding()->GetPadding(newValue) || oldValue != newValue) &&
          !props.Get(UsedPaddingProperty())) {
        props.Set(UsedPaddingProperty(), new nsMargin(oldValue));
      }
    }

    const nsStyleBorder* oldBorder = aOldStyleContext->PeekStyleBorder();
    if (oldBorder) {
      oldValue = oldBorder->GetActualBorder();
      newValue = GetStyleBorder()->GetActualBorder();
      if (oldValue != newValue &&
          !props.Get(UsedBorderProperty())) {
        props.Set(UsedBorderProperty(), new nsMargin(oldValue));
      }
    }
  }

  imgIRequest *oldBorderImage = aOldStyleContext
    ? aOldStyleContext->GetStyleBorder()->GetBorderImage()
    : nsnull;
  // For border-images, we can't be as conservative (we need to set the
  // new loaders if there has been any change) since the CalcDifference
  // call depended on the result of GetActualBorder() and that result
  // depends on whether the image has loaded, start the image load now
  // so that we'll get notified when it completes loading and can do a
  // restyle.  Otherwise, the image might finish loading from the
  // network before we start listening to its notifications, and then
  // we'll never know that it's finished loading.  Likewise, we want to
  // do this for freshly-created frames to prevent a similar race if the
  // image loads between reflow (which can depend on whether the image
  // is loaded) and paint.  We also don't really care about any callers
  // who try to paint borders with a different style context, because
  // they won't have the correct size for the border either.
  if (!EqualImages(oldBorderImage, GetStyleBorder()->GetBorderImage())) {
    // stop and restart the image loading/notification
    PresContext()->SetupBorderImageLoaders(this, GetStyleBorder());
  }

  // If the page contains markup that overrides text direction, and
  // does not contain any characters that would activate the Unicode
  // bidi algorithm, we need to call |SetBidiEnabled| on the pres
  // context before reflow starts.  See bug 115921.
  if (GetStyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL) {
    PresContext()->SetBidiEnabled();
  }
}

// MSVC fails with link error "one or more multiply defined symbols found",
// gcc fails with "hidden symbol `nsIFrame::kPrincipalList' isn't defined"
// etc if they are not defined.
#ifndef _MSC_VER
// static nsIFrame constants; initialized in the header file.
const nsIFrame::ChildListID nsIFrame::kPrincipalList;
const nsIFrame::ChildListID nsIFrame::kAbsoluteList;
const nsIFrame::ChildListID nsIFrame::kBulletList;
const nsIFrame::ChildListID nsIFrame::kCaptionList;
const nsIFrame::ChildListID nsIFrame::kColGroupList;
const nsIFrame::ChildListID nsIFrame::kExcessOverflowContainersList;
const nsIFrame::ChildListID nsIFrame::kFixedList;
const nsIFrame::ChildListID nsIFrame::kFloatList;
const nsIFrame::ChildListID nsIFrame::kOverflowContainersList;
const nsIFrame::ChildListID nsIFrame::kOverflowList;
const nsIFrame::ChildListID nsIFrame::kOverflowOutOfFlowList;
const nsIFrame::ChildListID nsIFrame::kPopupList;
const nsIFrame::ChildListID nsIFrame::kPushedFloatsList;
const nsIFrame::ChildListID nsIFrame::kSelectPopupList;
const nsIFrame::ChildListID nsIFrame::kNoReflowPrincipalList;
#endif

/* virtual */ nsMargin
nsIFrame::GetUsedMargin() const
{
  nsMargin margin(0, 0, 0, 0);
  if ((mState & NS_FRAME_FIRST_REFLOW) &&
      !(mState & NS_FRAME_IN_REFLOW))
    return margin;

  nsMargin *m = static_cast<nsMargin*>
                           (Properties().Get(UsedMarginProperty()));
  if (m) {
    margin = *m;
  } else {
#ifdef DEBUG
    PRBool hasMargin = 
#endif
    GetStyleMargin()->GetMargin(margin);
    NS_ASSERTION(hasMargin, "We should have a margin here! (out of memory?)");
  }
  return margin;
}

/* virtual */ nsMargin
nsIFrame::GetUsedBorder() const
{
  nsMargin border(0, 0, 0, 0);
  if ((mState & NS_FRAME_FIRST_REFLOW) &&
      !(mState & NS_FRAME_IN_REFLOW))
    return border;

  // Theme methods don't use const-ness.
  nsIFrame *mutable_this = const_cast<nsIFrame*>(this);

  const nsStyleDisplay *disp = GetStyleDisplay();
  if (mutable_this->IsThemed(disp)) {
    nsIntMargin result;
    nsPresContext *presContext = PresContext();
    presContext->GetTheme()->GetWidgetBorder(presContext->DeviceContext(),
                                             mutable_this, disp->mAppearance,
                                             &result);
    border.left = presContext->DevPixelsToAppUnits(result.left);
    border.top = presContext->DevPixelsToAppUnits(result.top);
    border.right = presContext->DevPixelsToAppUnits(result.right);
    border.bottom = presContext->DevPixelsToAppUnits(result.bottom);
    return border;
  }

  nsMargin *b = static_cast<nsMargin*>
                           (Properties().Get(UsedBorderProperty()));
  if (b) {
    border = *b;
  } else {
    border = GetStyleBorder()->GetActualBorder();
  }
  return border;
}

/* virtual */ nsMargin
nsIFrame::GetUsedPadding() const
{
  nsMargin padding(0, 0, 0, 0);
  if ((mState & NS_FRAME_FIRST_REFLOW) &&
      !(mState & NS_FRAME_IN_REFLOW))
    return padding;

  // Theme methods don't use const-ness.
  nsIFrame *mutable_this = const_cast<nsIFrame*>(this);

  const nsStyleDisplay *disp = GetStyleDisplay();
  if (mutable_this->IsThemed(disp)) {
    nsPresContext *presContext = PresContext();
    nsIntMargin widget;
    if (presContext->GetTheme()->GetWidgetPadding(presContext->DeviceContext(),
                                                  mutable_this,
                                                  disp->mAppearance,
                                                  &widget)) {
      padding.top = presContext->DevPixelsToAppUnits(widget.top);
      padding.right = presContext->DevPixelsToAppUnits(widget.right);
      padding.bottom = presContext->DevPixelsToAppUnits(widget.bottom);
      padding.left = presContext->DevPixelsToAppUnits(widget.left);
      return padding;
    }
  }

  nsMargin *p = static_cast<nsMargin*>
                           (Properties().Get(UsedPaddingProperty()));
  if (p) {
    padding = *p;
  } else {
#ifdef DEBUG
    PRBool hasPadding = 
#endif
    GetStylePadding()->GetPadding(padding);
    NS_ASSERTION(hasPadding, "We should have padding here! (out of memory?)");
  }
  return padding;
}

void
nsIFrame::ApplySkipSides(nsMargin& aMargin) const
{
  PRIntn skipSides = GetSkipSides();
  if (skipSides & (1 << NS_SIDE_TOP))
    aMargin.top = 0;
  if (skipSides & (1 << NS_SIDE_RIGHT))
    aMargin.right = 0;
  if (skipSides & (1 << NS_SIDE_BOTTOM))
    aMargin.bottom = 0;
  if (skipSides & (1 << NS_SIDE_LEFT))
    aMargin.left = 0;
}

nsRect
nsIFrame::GetPaddingRectRelativeToSelf() const
{
  nsMargin bp(GetUsedBorder());
  ApplySkipSides(bp);
  nsRect r(0, 0, mRect.width, mRect.height);
  r.Deflate(bp);
  return r;
}

nsRect
nsIFrame::GetPaddingRect() const
{
  return GetPaddingRectRelativeToSelf() + GetPosition();
}

PRBool
nsIFrame::IsTransformed() const
{
  return (mState & NS_FRAME_MAY_BE_TRANSFORMED) &&
    GetStyleDisplay()->HasTransform();
}

PRBool
nsIFrame::Preserves3DChildren() const
{
  if (GetStyleDisplay()->mTransformStyle != NS_STYLE_TRANSFORM_STYLE_PRESERVE_3D || !IsTransformed())
      return PR_FALSE;

  // If we're all scroll frame, then all descendants will be clipped, so we can't preserve 3d.
  if (GetType() == nsGkAtoms::scrollFrame)
      return PR_FALSE;

  nsRect temp;
  return (!ApplyOverflowClipping(nsnull, this, GetStyleDisplay(), &temp) &&
      !ApplyAbsPosClipping(nsnull, GetStyleDisplay(), this, &temp) &&
      !nsSVGIntegrationUtils::UsingEffectsForFrame(this));
}

PRBool
nsIFrame::Preserves3D() const
{
  if (!GetParent() || !GetParent()->Preserves3DChildren() || !IsTransformed()) {
    return PR_FALSE;
  }
  return PR_TRUE;
}

nsRect
nsIFrame::GetContentRectRelativeToSelf() const
{
  nsMargin bp(GetUsedBorderAndPadding());
  ApplySkipSides(bp);
  nsRect r(0, 0, mRect.width, mRect.height);
  r.Deflate(bp);
  return r;
}

nsRect
nsIFrame::GetContentRect() const
{
  return GetContentRectRelativeToSelf() + GetPosition();
}

PRBool
nsIFrame::ComputeBorderRadii(const nsStyleCorners& aBorderRadius,
                             const nsSize& aFrameSize,
                             const nsSize& aBorderArea,
                             PRIntn aSkipSides,
                             nscoord aRadii[8])
{
  // Percentages are relative to whichever side they're on.
  NS_FOR_CSS_HALF_CORNERS(i) {
    const nsStyleCoord c = aBorderRadius.Get(i);
    nscoord axis =
      NS_HALF_CORNER_IS_X(i) ? aFrameSize.width : aFrameSize.height;

    if (c.IsCoordPercentCalcUnit()) {
      aRadii[i] = nsRuleNode::ComputeCoordPercentCalc(c, axis);
      if (aRadii[i] < 0) {
        // clamp calc()
        aRadii[i] = 0;
      }
    } else {
      NS_NOTREACHED("ComputeBorderRadii: bad unit");
      aRadii[i] = 0;
    }
  }

  if (aSkipSides & (1 << NS_SIDE_TOP)) {
    aRadii[NS_CORNER_TOP_LEFT_X] = 0;
    aRadii[NS_CORNER_TOP_LEFT_Y] = 0;
    aRadii[NS_CORNER_TOP_RIGHT_X] = 0;
    aRadii[NS_CORNER_TOP_RIGHT_Y] = 0;
  }

  if (aSkipSides & (1 << NS_SIDE_RIGHT)) {
    aRadii[NS_CORNER_TOP_RIGHT_X] = 0;
    aRadii[NS_CORNER_TOP_RIGHT_Y] = 0;
    aRadii[NS_CORNER_BOTTOM_RIGHT_X] = 0;
    aRadii[NS_CORNER_BOTTOM_RIGHT_Y] = 0;
  }

  if (aSkipSides & (1 << NS_SIDE_BOTTOM)) {
    aRadii[NS_CORNER_BOTTOM_RIGHT_X] = 0;
    aRadii[NS_CORNER_BOTTOM_RIGHT_Y] = 0;
    aRadii[NS_CORNER_BOTTOM_LEFT_X] = 0;
    aRadii[NS_CORNER_BOTTOM_LEFT_Y] = 0;
  }

  if (aSkipSides & (1 << NS_SIDE_LEFT)) {
    aRadii[NS_CORNER_BOTTOM_LEFT_X] = 0;
    aRadii[NS_CORNER_BOTTOM_LEFT_Y] = 0;
    aRadii[NS_CORNER_TOP_LEFT_X] = 0;
    aRadii[NS_CORNER_TOP_LEFT_Y] = 0;
  }

  // css3-background specifies this algorithm for reducing
  // corner radii when they are too big.
  PRBool haveRadius = PR_FALSE;
  double ratio = 1.0f;
  NS_FOR_CSS_SIDES(side) {
    PRUint32 hc1 = NS_SIDE_TO_HALF_CORNER(side, PR_FALSE, PR_TRUE);
    PRUint32 hc2 = NS_SIDE_TO_HALF_CORNER(side, PR_TRUE, PR_TRUE);
    nscoord length =
      NS_SIDE_IS_VERTICAL(side) ? aBorderArea.height : aBorderArea.width;
    nscoord sum = aRadii[hc1] + aRadii[hc2];
    if (sum)
      haveRadius = PR_TRUE;

    // avoid floating point division in the normal case
    if (length < sum)
      ratio = NS_MIN(ratio, double(length)/sum);
  }
  if (ratio < 1.0) {
    NS_FOR_CSS_HALF_CORNERS(corner) {
      aRadii[corner] *= ratio;
    }
  }

  return haveRadius;
}

/* static */ void
nsIFrame::InsetBorderRadii(nscoord aRadii[8], const nsMargin &aOffsets)
{
  NS_FOR_CSS_SIDES(side) {
    nscoord offset = aOffsets.Side(side);
    PRUint32 hc1 = NS_SIDE_TO_HALF_CORNER(side, PR_FALSE, PR_FALSE);
    PRUint32 hc2 = NS_SIDE_TO_HALF_CORNER(side, PR_TRUE, PR_FALSE);
    aRadii[hc1] = NS_MAX(0, aRadii[hc1] - offset);
    aRadii[hc2] = NS_MAX(0, aRadii[hc2] - offset);
  }
}

/* static */ void
nsIFrame::OutsetBorderRadii(nscoord aRadii[8], const nsMargin &aOffsets)
{
  NS_FOR_CSS_SIDES(side) {
    nscoord offset = aOffsets.Side(side);
    PRUint32 hc1 = NS_SIDE_TO_HALF_CORNER(side, PR_FALSE, PR_FALSE);
    PRUint32 hc2 = NS_SIDE_TO_HALF_CORNER(side, PR_TRUE, PR_FALSE);
    if (aRadii[hc1] > 0)
      aRadii[hc1] += offset;
    if (aRadii[hc2] > 0)
      aRadii[hc2] += offset;
  }
}

/* virtual */ PRBool
nsIFrame::GetBorderRadii(nscoord aRadii[8]) const
{
  if (IsThemed()) {
    // When we're themed, the native theme code draws the border and
    // background, and therefore it doesn't make sense to tell other
    // code that's interested in border-radius that we have any radii.
    //
    // In an ideal world, we might have a way for the them to tell us an
    // border radius, but since we don't, we're better off assuming
    // zero.
    NS_FOR_CSS_HALF_CORNERS(corner) {
      aRadii[corner] = 0;
    }
    return PR_FALSE;
  }
  nsSize size = GetSize();
  return ComputeBorderRadii(GetStyleBorder()->mBorderRadius, size, size,
                            GetSkipSides(), aRadii);
}

PRBool
nsIFrame::GetPaddingBoxBorderRadii(nscoord aRadii[8]) const
{
  if (!GetBorderRadii(aRadii))
    return PR_FALSE;
  InsetBorderRadii(aRadii, GetUsedBorder());
  NS_FOR_CSS_HALF_CORNERS(corner) {
    if (aRadii[corner])
      return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool
nsIFrame::GetContentBoxBorderRadii(nscoord aRadii[8]) const
{
  if (!GetBorderRadii(aRadii))
    return PR_FALSE;
  InsetBorderRadii(aRadii, GetUsedBorderAndPadding());
  NS_FOR_CSS_HALF_CORNERS(corner) {
    if (aRadii[corner])
      return PR_TRUE;
  }
  return PR_FALSE;
}

nsStyleContext*
nsFrame::GetAdditionalStyleContext(PRInt32 aIndex) const
{
  NS_PRECONDITION(aIndex >= 0, "invalid index number");
  return nsnull;
}

void
nsFrame::SetAdditionalStyleContext(PRInt32 aIndex, 
                                   nsStyleContext* aStyleContext)
{
  NS_PRECONDITION(aIndex >= 0, "invalid index number");
}

nscoord
nsFrame::GetBaseline() const
{
  NS_ASSERTION(!NS_SUBTREE_DIRTY(this),
               "frame must not be dirty");
  // Default to the bottom margin edge, per CSS2.1's definition of the
  // 'baseline' value of 'vertical-align'.
  return mRect.height + GetUsedMargin().bottom;
}

static nsIFrame*
GetActiveSelectionFrame(nsPresContext* aPresContext, nsIFrame* aFrame)
{
  nsIContent* capturingContent = nsIPresShell::GetCapturingContent();
  if (capturingContent) {
    nsIFrame* activeFrame = aPresContext->GetPrimaryFrameFor(capturingContent);
    return activeFrame ? activeFrame : aFrame;
  }

  return aFrame;
}

PRInt16
nsFrame::DisplaySelection(nsPresContext* aPresContext, PRBool isOkToTurnOn)
{
  PRInt16 selType = nsISelectionController::SELECTION_OFF;

  nsCOMPtr<nsISelectionController> selCon;
  nsresult result = GetSelectionController(aPresContext, getter_AddRefs(selCon));
  if (NS_SUCCEEDED(result) && selCon) {
    result = selCon->GetDisplaySelection(&selType);
    if (NS_SUCCEEDED(result) && (selType != nsISelectionController::SELECTION_OFF)) {
      // Check whether style allows selection.
      PRBool selectable;
      IsSelectable(&selectable, nsnull);
      if (!selectable) {
        selType = nsISelectionController::SELECTION_OFF;
        isOkToTurnOn = PR_FALSE;
      }
    }
    if (isOkToTurnOn && (selType == nsISelectionController::SELECTION_OFF)) {
      selCon->SetDisplaySelection(nsISelectionController::SELECTION_ON);
      selType = nsISelectionController::SELECTION_ON;
    }
  }
  return selType;
}

class nsDisplaySelectionOverlay : public nsDisplayItem {
public:
  nsDisplaySelectionOverlay(nsDisplayListBuilder* aBuilder,
                            nsFrame* aFrame, PRInt16 aSelectionValue)
    : nsDisplayItem(aBuilder, aFrame), mSelectionValue(aSelectionValue) {
    MOZ_COUNT_CTOR(nsDisplaySelectionOverlay);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplaySelectionOverlay() {
    MOZ_COUNT_DTOR(nsDisplaySelectionOverlay);
  }
#endif

  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx);
  NS_DISPLAY_DECL_NAME("SelectionOverlay", TYPE_SELECTION_OVERLAY)
private:
  PRInt16 mSelectionValue;
};

void nsDisplaySelectionOverlay::Paint(nsDisplayListBuilder* aBuilder,
                                      nsRenderingContext* aCtx)
{
  nscolor color = NS_RGB(255, 255, 255);
  
  nsILookAndFeel::nsColorID colorID;
  nsresult result;
  if (mSelectionValue == nsISelectionController::SELECTION_ON) {
    colorID = nsILookAndFeel::eColor_TextSelectBackground;
  } else if (mSelectionValue == nsISelectionController::SELECTION_ATTENTION) {
    colorID = nsILookAndFeel::eColor_TextSelectBackgroundAttention;
  } else {
    colorID = nsILookAndFeel::eColor_TextSelectBackgroundDisabled;
  }

  nsCOMPtr<nsILookAndFeel> look;
  look = do_GetService(kLookAndFeelCID, &result);
  if (NS_SUCCEEDED(result) && look)
    look->GetColor(colorID, color);

  gfxRGBA c(color);
  c.a = .5;

  gfxContext *ctx = aCtx->ThebesContext();
  ctx->SetColor(c);

  nsIntRect pxRect =
    mVisibleRect.ToOutsidePixels(mFrame->PresContext()->AppUnitsPerDevPixel());
  ctx->NewPath();
  ctx->Rectangle(gfxRect(pxRect.x, pxRect.y, pxRect.width, pxRect.height), PR_TRUE);
  ctx->Fill();
}

/********************************************************
* Refreshes each content's frame
*********************************************************/

nsresult
nsFrame::DisplaySelectionOverlay(nsDisplayListBuilder*   aBuilder,
                                 nsDisplayList*          aList,
                                 PRUint16                aContentType)
{
//check frame selection state
  if ((GetStateBits() & NS_FRAME_SELECTED_CONTENT) != NS_FRAME_SELECTED_CONTENT)
    return NS_OK;
  if (!IsVisibleForPainting(aBuilder))
    return NS_OK;
    
  nsPresContext* presContext = PresContext();
  nsIPresShell *shell = presContext->PresShell();
  if (!shell)
    return NS_OK;

  PRInt16 displaySelection = shell->GetSelectionFlags();
  if (!(displaySelection & aContentType))
    return NS_OK;

  const nsFrameSelection* frameSelection = GetConstFrameSelection();
  PRInt16 selectionValue = frameSelection->GetDisplaySelection();

  if (selectionValue <= nsISelectionController::SELECTION_HIDDEN)
    return NS_OK; // selection is hidden or off

  nsIContent *newContent = mContent->GetParent();

  //check to see if we are anonymous content
  PRInt32 offset = 0;
  if (newContent) {
    // XXXbz there has GOT to be a better way of determining this!
    offset = newContent->IndexOf(mContent);
  }

  SelectionDetails *details;
  //look up to see what selection(s) are on this frame
  details = frameSelection->LookUpSelection(newContent, offset, 1, PR_FALSE);
  // XXX is the above really necessary? We don't actually DO anything
  // with the details other than test that they're non-null
  if (!details)
    return NS_OK;
  
  while (details) {
    SelectionDetails *next = details->mNext;
    delete details;
    details = next;
  }

  return aList->AppendNewToTop(new (aBuilder)
      nsDisplaySelectionOverlay(aBuilder, this, selectionValue));
}

nsresult
nsFrame::DisplayOutlineUnconditional(nsDisplayListBuilder*   aBuilder,
                                     const nsDisplayListSet& aLists)
{
  if (GetStyleOutline()->GetOutlineStyle() == NS_STYLE_BORDER_STYLE_NONE)
    return NS_OK;
    
  return aLists.Outlines()->AppendNewToTop(
      new (aBuilder) nsDisplayOutline(aBuilder, this));
}

nsresult
nsFrame::DisplayOutline(nsDisplayListBuilder*   aBuilder,
                        const nsDisplayListSet& aLists)
{
  if (!IsVisibleForPainting(aBuilder))
    return NS_OK;

  return DisplayOutlineUnconditional(aBuilder, aLists);
}

nsresult
nsIFrame::DisplayCaret(nsDisplayListBuilder* aBuilder,
                       const nsRect& aDirtyRect, nsDisplayList* aList)
{
  if (!IsVisibleForPainting(aBuilder))
    return NS_OK;

  return aList->AppendNewToTop(
      new (aBuilder) nsDisplayCaret(aBuilder, this, aBuilder->GetCaret()));
}

nscolor
nsIFrame::GetCaretColorAt(PRInt32 aOffset)
{
  // Use text color.
  return GetStyleColor()->mColor;
}

PRBool
nsIFrame::HasBorder() const
{
  // Border images contribute to the background of the content area
  // even if there's no border proper.
  return (GetUsedBorder() != nsMargin(0,0,0,0) ||
          GetStyleBorder()->IsBorderImageLoaded());
}

nsresult
nsFrame::DisplayBackgroundUnconditional(nsDisplayListBuilder*   aBuilder,
                                        const nsDisplayListSet& aLists,
                                        PRBool                  aForceBackground)
{
  // Here we don't try to detect background propagation. Frames that might
  // receive a propagated background should just set aForceBackground to
  // PR_TRUE.
  if (aBuilder->IsForEventDelivery() || aForceBackground ||
      !GetStyleBackground()->IsTransparent() || GetStyleDisplay()->mAppearance) {
    return aLists.BorderBackground()->AppendNewToTop(new (aBuilder)
        nsDisplayBackground(aBuilder, this));
  }
  return NS_OK;
}

nsresult
nsFrame::DisplayBorderBackgroundOutline(nsDisplayListBuilder*   aBuilder,
                                        const nsDisplayListSet& aLists,
                                        PRBool                  aForceBackground)
{
  // The visibility check belongs here since child elements have the
  // opportunity to override the visibility property and display even if
  // their parent is hidden.
  if (!IsVisibleForPainting(aBuilder))
    return NS_OK;

  PRBool hasBoxShadow = GetStyleBorder()->mBoxShadow != nsnull;
  if (hasBoxShadow) {
    nsresult rv = aLists.BorderBackground()->AppendNewToTop(new (aBuilder)
        nsDisplayBoxShadowOuter(aBuilder, this));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsresult rv =
    DisplayBackgroundUnconditional(aBuilder, aLists, aForceBackground);
  NS_ENSURE_SUCCESS(rv, rv);

  if (hasBoxShadow) {
    rv = aLists.BorderBackground()->AppendNewToTop(new (aBuilder)
        nsDisplayBoxShadowInner(aBuilder, this));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  
  if (HasBorder()) {
    rv = aLists.BorderBackground()->AppendNewToTop(new (aBuilder)
        nsDisplayBorder(aBuilder, this));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return DisplayOutlineUnconditional(aBuilder, aLists);
}

PRBool
nsIFrame::GetAbsPosClipRect(const nsStyleDisplay* aDisp, nsRect* aRect,
                            const nsSize& aSize) const
{
  NS_PRECONDITION(aRect, "Must have aRect out parameter");

  if (!aDisp->IsAbsolutelyPositioned() ||
      !(aDisp->mClipFlags & NS_STYLE_CLIP_RECT))
    return PR_FALSE;

  *aRect = aDisp->mClip;
  if (NS_STYLE_CLIP_RIGHT_AUTO & aDisp->mClipFlags) {
    aRect->width = aSize.width - aRect->x;
  }
  if (NS_STYLE_CLIP_BOTTOM_AUTO & aDisp->mClipFlags) {
    aRect->height = aSize.height - aRect->y;
  }
  return PR_TRUE;
}

static PRBool ApplyAbsPosClipping(nsDisplayListBuilder* aBuilder,
                                  const nsStyleDisplay* aDisp, const nsIFrame* aFrame,
                                  nsRect* aRect) {
  if (!aFrame->GetAbsPosClipRect(aDisp, aRect, aFrame->GetSize()))
    return PR_FALSE;

  if (aBuilder) {
    *aRect += aBuilder->ToReferenceFrame(aFrame);
  }
  return PR_TRUE;
}

/**
 * Returns PR_TRUE if aFrame is overflow:hidden and we should interpret
 * that as -moz-hidden-unscrollable.
 */
static inline PRBool ApplyOverflowHiddenClipping(const nsIFrame* aFrame,
                                                 const nsStyleDisplay* aDisp)
{
  if (aDisp->mOverflowX != NS_STYLE_OVERFLOW_HIDDEN)
    return PR_FALSE;
    
  nsIAtom* type = aFrame->GetType();
  // REVIEW: these are the frame types that call IsTableClip and set up
  // clipping. Actually there were also table rows and the inner table frame
  // doing this, but 'overflow' isn't applicable to them according to
  // CSS 2.1 so I removed them. Also, we used to clip at tableOuterFrame
  // but we should actually clip at tableFrame (as per discussion with Hixie and
  // bz).
  return type == nsGkAtoms::tableFrame ||
       type == nsGkAtoms::tableCellFrame ||
       type == nsGkAtoms::bcTableCellFrame;
}

static PRBool ApplyOverflowClipping(nsDisplayListBuilder* aBuilder,
                                    const nsIFrame* aFrame,
                                    const nsStyleDisplay* aDisp, nsRect* aRect) {
  // REVIEW: from nsContainerFrame.cpp SyncFrameViewGeometryDependentProperties,
  // except that that function used the border-edge for
  // -moz-hidden-unscrollable which I don't think is correct... Also I've
  // changed -moz-hidden-unscrollable to apply to any kind of frame.

  // Only -moz-hidden-unscrollable is handled here (and 'hidden' for table
  // frames, and any non-visible value for blocks in a paginated context).
  // Other overflow clipping is applied by nsHTML/XULScrollFrame.
  if (!ApplyOverflowHiddenClipping(aFrame, aDisp) &&
      !nsFrame::ApplyPaginatedOverflowClipping(aFrame)) {
    PRBool clip = aDisp->mOverflowX == NS_STYLE_OVERFLOW_CLIP;
    if (!clip)
      return PR_FALSE;
    // We allow -moz-hidden-unscrollable to apply to any kind of frame. This
    // is required by comboboxes which make their display text (an inline frame)
    // have clipping.
  }
  
  *aRect = aFrame->GetPaddingRect() - aFrame->GetPosition();
  if (aBuilder) {
    *aRect += aBuilder->ToReferenceFrame(aFrame);
  }
  return PR_TRUE;
}

class nsOverflowClipWrapper : public nsDisplayWrapper
{
public:
  /**
   * Create a wrapper to apply overflow clipping for aContainer.
   * @param aClipBorderBackground set to PR_TRUE to clip the BorderBackground()
   * list, otherwise it will not be clipped
   * @param aClipAll set to PR_TRUE to clip all descendants, even those for
   * which we aren't the containing block
   */
  nsOverflowClipWrapper(nsIFrame* aContainer, const nsRect& aRect,
                        const nscoord aRadii[8],
                        PRBool aClipBorderBackground, PRBool aClipAll)
    : mContainer(aContainer), mRect(aRect),
      mClipBorderBackground(aClipBorderBackground), mClipAll(aClipAll),
      mHaveRadius(PR_FALSE)
  {
    memcpy(mRadii, aRadii, sizeof(mRadii));
    NS_FOR_CSS_HALF_CORNERS(corner) {
      if (aRadii[corner] > 0) {
        mHaveRadius = PR_TRUE;
        break;
      }
    }
  }
  virtual PRBool WrapBorderBackground() { return mClipBorderBackground; }
  virtual nsDisplayItem* WrapList(nsDisplayListBuilder* aBuilder,
                                  nsIFrame* aFrame, nsDisplayList* aList) {
    // We are not a stacking context root. There is no valid underlying
    // frame for the whole list. These items are all in-flow descendants so
    // we can safely just clip them.
    if (mHaveRadius) {
      return new (aBuilder) nsDisplayClipRoundedRect(aBuilder, nsnull, aList,
                                                     mRect, mRadii);
    }
    return new (aBuilder) nsDisplayClip(aBuilder, nsnull, aList, mRect);
  }
  virtual nsDisplayItem* WrapItem(nsDisplayListBuilder* aBuilder,
                                  nsDisplayItem* aItem) {
    nsIFrame* f = aItem->GetUnderlyingFrame();
    if (mClipAll ||
        nsLayoutUtils::IsProperAncestorFrame(mContainer, f, nsnull)) {
      if (mHaveRadius) {
        return new (aBuilder) nsDisplayClipRoundedRect(aBuilder, f, aItem,
                                                       mRect, mRadii);
      }
      return new (aBuilder) nsDisplayClip(aBuilder, f, aItem, mRect);
    }
    return aItem;
  }
protected:
  nsIFrame*    mContainer;
  nsRect       mRect;
  nscoord      mRadii[8];
  PRPackedBool mClipBorderBackground;
  PRPackedBool mClipAll;
  PRPackedBool mHaveRadius;
};

class nsAbsPosClipWrapper : public nsDisplayWrapper
{
public:
  nsAbsPosClipWrapper(const nsRect& aRect)
    : mRect(aRect) {}
  virtual nsDisplayItem* WrapList(nsDisplayListBuilder* aBuilder,
                                  nsIFrame* aFrame, nsDisplayList* aList) {
    // We are not a stacking context root. There is no valid underlying
    // frame for the whole list.
    return new (aBuilder) nsDisplayClip(aBuilder, nsnull, aList, mRect);
  }
  virtual nsDisplayItem* WrapItem(nsDisplayListBuilder* aBuilder,
                                  nsDisplayItem* aItem) {
    return new (aBuilder) nsDisplayClip(aBuilder, aItem->GetUnderlyingFrame(),
                                        aItem, mRect);
  }
protected:
  nsRect    mRect;
};

nsresult
nsIFrame::OverflowClip(nsDisplayListBuilder*   aBuilder,
                       const nsDisplayListSet& aFromSet,
                       const nsDisplayListSet& aToSet,
                       const nsRect&           aClipRect,
                       const nscoord           aClipRadii[8],
                       PRBool                  aClipBorderBackground,
                       PRBool                  aClipAll)
{
  nsOverflowClipWrapper wrapper(this, aClipRect, aClipRadii,
                                aClipBorderBackground, aClipAll);
  return wrapper.WrapLists(aBuilder, this, aFromSet, aToSet);
}

static nsresult
BuildDisplayListWithOverflowClip(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
    const nsRect& aDirtyRect, const nsDisplayListSet& aSet,
    const nsRect& aClipRect, const nscoord aClipRadii[8])
{
  nsDisplayListCollection set;
  nsresult rv = aFrame->BuildDisplayList(aBuilder, aDirtyRect, set);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aBuilder->DisplayCaret(aFrame, aDirtyRect, aSet.Content());
  NS_ENSURE_SUCCESS(rv, rv);

  return aFrame->OverflowClip(aBuilder, set, aSet, aClipRect, aClipRadii);
}

#ifdef NS_DEBUG
static void PaintDebugBorder(nsIFrame* aFrame, nsRenderingContext* aCtx,
     const nsRect& aDirtyRect, nsPoint aPt) {
  nsRect r(aPt, aFrame->GetSize());
  if (aFrame->HasView()) {
    aCtx->SetColor(NS_RGB(0,0,255));
  } else {
    aCtx->SetColor(NS_RGB(255,0,0));
  }
  aCtx->DrawRect(r);
}

static void PaintEventTargetBorder(nsIFrame* aFrame, nsRenderingContext* aCtx,
     const nsRect& aDirtyRect, nsPoint aPt) {
  nsRect r(aPt, aFrame->GetSize());
  aCtx->SetColor(NS_RGB(128,0,128));
  aCtx->DrawRect(r);
}

static void
DisplayDebugBorders(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                    const nsDisplayListSet& aLists) {
  // Draw a border around the child
  // REVIEW: From nsContainerFrame::PaintChild
  if (nsFrame::GetShowFrameBorders() && !aFrame->GetRect().IsEmpty()) {
    aLists.Outlines()->AppendNewToTop(new (aBuilder)
        nsDisplayGeneric(aBuilder, aFrame, PaintDebugBorder, "DebugBorder",
                         nsDisplayItem::TYPE_DEBUG_BORDER));
  }
  // Draw a border around the current event target
  if (nsFrame::GetShowEventTargetFrameBorder() &&
      aFrame->PresContext()->PresShell()->GetDrawEventTargetFrame() == aFrame) {
    aLists.Outlines()->AppendNewToTop(new (aBuilder)
        nsDisplayGeneric(aBuilder, aFrame, PaintEventTargetBorder, "EventTargetBorder",
                         nsDisplayItem::TYPE_EVENT_TARGET_BORDER));
  }
}
#endif

static nsresult
WrapPreserve3DList(nsIFrame *aFrame, nsDisplayListBuilder *aBuilder, nsDisplayList *aList)
{
  nsresult rv = NS_OK;
  nsDisplayList newList;
  while (nsDisplayItem *item = aList->RemoveBottom()) {
    nsIFrame *childFrame = item->GetUnderlyingFrame();
    NS_ASSERTION(childFrame, "All display items to be wrapped must have a frame!");
    if (childFrame->GetParent()->Preserves3DChildren()) {
      switch (item->GetType()) {
        case nsDisplayItem::TYPE_TRANSFORM: {
          // The child transform frame should always preserve 3d. In the cases where preserve-3d is disabled
          // such as clipping, this would be wrapped in a clip display object, and we wouldn't reach this point.
          NS_ASSERTION(childFrame->Preserves3D(), "Child transform frame must preserve 3d!");
          break;
        }
        case nsDisplayItem::TYPE_WRAP_LIST: {
          nsDisplayWrapList *list = static_cast<nsDisplayWrapList*>(item);
          rv = WrapPreserve3DList(aFrame, aBuilder, list->GetList());
          break;
        }
        case nsDisplayItem::TYPE_OPACITY: {
          nsDisplayOpacity *opacity = static_cast<nsDisplayOpacity*>(item);
          rv = WrapPreserve3DList(aFrame, aBuilder, opacity->GetList());
          break;
        }
        default: {
          item = new (aBuilder) nsDisplayTransform(aBuilder, childFrame, item);
          break;
        }
      } 
    } else {
      item = new (aBuilder) nsDisplayTransform(aBuilder, childFrame, item);
    }
 
    if (NS_FAILED(rv) || !item)
      return rv;

    newList.AppendToTop(item);
  }

  aList->AppendToTop(&newList);
  return NS_OK;
}

nsresult
nsIFrame::BuildDisplayListForStackingContext(nsDisplayListBuilder* aBuilder,
                                             const nsRect&         aDirtyRect,
                                             nsDisplayList*        aList) {
  if (GetStateBits() & NS_FRAME_TOO_DEEP_IN_FRAME_TREE)
    return NS_OK;

  // Replaced elements have their visibility handled here, because
  // they're visually atomic
  if (IsFrameOfType(eReplaced) && !IsVisibleForPainting(aBuilder))
    return NS_OK;

  nsRect absPosClip;
  const nsStyleDisplay* disp = GetStyleDisplay();
  // We can stop right away if this is a zero-opacity stacking context and
  // we're painting.
  if (disp->mOpacity == 0.0 && aBuilder->IsForPainting())
    return NS_OK;

  PRBool applyAbsPosClipping =
      ApplyAbsPosClipping(aBuilder, disp, this, &absPosClip);
  nsRect dirtyRect = aDirtyRect;

  PRBool inTransform = aBuilder->IsInTransform();
  /* If we're being transformed, we need to invert the matrix transform so that we don't 
   * grab points in the wrong coordinate system!
   */
  if ((mState & NS_FRAME_MAY_BE_TRANSFORMED) &&
      disp->HasTransform()) {
    /* If we have a complex transform, just grab the entire overflow rect instead. */
    if (Preserves3DChildren() || !nsDisplayTransform::UntransformRect(dirtyRect, this, nsPoint(0, 0), &dirtyRect)) {
      dirtyRect = GetVisualOverflowRectRelativeToSelf();
    }
    inTransform = PR_TRUE;
  }

  if (applyAbsPosClipping) {
    dirtyRect.IntersectRect(dirtyRect,
                            absPosClip - aBuilder->ToReferenceFrame(this));
  }

  PRBool usingSVGEffects = nsSVGIntegrationUtils::UsingEffectsForFrame(this);
  if (usingSVGEffects) {
    dirtyRect =
      nsSVGIntegrationUtils::GetRequiredSourceForInvalidArea(this, dirtyRect);
  }

  nsDisplayListCollection set;
  nsresult rv;
  {    
    nsDisplayListBuilder::AutoIsRootSetter rootSetter(aBuilder, PR_TRUE);
    nsDisplayListBuilder::AutoInTransformSetter
      inTransformSetter(aBuilder, inTransform);
    rv = BuildDisplayList(aBuilder, dirtyRect, set);
  }
  NS_ENSURE_SUCCESS(rv, rv);
    
  if (aBuilder->IsBackgroundOnly()) {
    set.BlockBorderBackgrounds()->DeleteAll();
    set.Floats()->DeleteAll();
    set.Content()->DeleteAll();
    set.PositionedDescendants()->DeleteAll();
    set.Outlines()->DeleteAll();
  }
  
  // This z-order sort also sorts secondarily by content order. We need to do
  // this so that boxes produced by the same element are placed together
  // in the sort. Consider a position:relative inline element that breaks
  // across lines and has absolutely positioned children; all the abs-pos
  // children should be z-ordered after all the boxes for the position:relative
  // element itself.
  set.PositionedDescendants()->SortByZOrder(aBuilder, GetContent());
  
  nsRect overflowClip;
  if (ApplyOverflowClipping(aBuilder, this, disp, &overflowClip)) {
    nscoord radii[8];
    this->GetPaddingBoxBorderRadii(radii);
    nsOverflowClipWrapper wrapper(this, overflowClip, radii,
                                  PR_FALSE, PR_FALSE);
    rv = wrapper.WrapListsInPlace(aBuilder, this, set);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  // We didn't use overflowClip to restrict the dirty rect, since some of the
  // descendants may not be clipped by it. Even if we end up with unnecessary
  // display items, they'll be pruned during ComputeVisibility.  

  nsDisplayList resultList;
  // Now follow the rules of http://www.w3.org/TR/CSS21/zindex.html
  // 1,2: backgrounds and borders
  resultList.AppendToTop(set.BorderBackground());
  // 3: negative z-index children.
  for (;;) {
    nsDisplayItem* item = set.PositionedDescendants()->GetBottom();
    if (item) {
      nsIFrame* f = item->GetUnderlyingFrame();
      NS_ASSERTION(f, "After sorting, every item in the list should have an underlying frame");
      if (nsLayoutUtils::GetZIndex(f) < 0) {
        set.PositionedDescendants()->RemoveBottom();
        resultList.AppendToTop(item);
        continue;
      }
    }
    break;
  }
  // 4: block backgrounds
  resultList.AppendToTop(set.BlockBorderBackgrounds());
  // 5: floats
  resultList.AppendToTop(set.Floats());
  // 7: general content
  resultList.AppendToTop(set.Content());
  // 7.5: outlines, in content tree order. We need to sort by content order
  // because an element with outline that breaks and has children with outline
  // might have placed child outline items between its own outline items.
  // The element's outline items need to all come before any child outline
  // items.
  set.Outlines()->SortByContentOrder(aBuilder, GetContent());
#ifdef NS_DEBUG
  DisplayDebugBorders(aBuilder, this, set);
#endif
  resultList.AppendToTop(set.Outlines());
  // 8, 9: non-negative z-index children
  resultList.AppendToTop(set.PositionedDescendants());

  if (applyAbsPosClipping) {
    nsAbsPosClipWrapper wrapper(absPosClip);
    nsDisplayItem* item = wrapper.WrapList(aBuilder, this, &resultList);
    if (!item)
      return NS_ERROR_OUT_OF_MEMORY;
    // resultList was emptied
    resultList.AppendToTop(item);
  }
 
  /* If there are any SVG effects, wrap up the list in an effects list. */
  if (usingSVGEffects) {
    /* List now emptied, so add the new list to the top. */
    rv = resultList.AppendNewToTop(
        new (aBuilder) nsDisplaySVGEffects(aBuilder, this, &resultList));
    if (NS_FAILED(rv))
      return rv;
  } else

  /* If there is any opacity, wrap it up in an opacity list.
   * If there's nothing in the list, don't add anything.
   */
  if (disp->mOpacity < 1.0f && !resultList.IsEmpty()) {
    rv = resultList.AppendNewToTop(
        new (aBuilder) nsDisplayOpacity(aBuilder, this, &resultList));
    if (NS_FAILED(rv))
      return rv;
  }

  /* If we're going to apply a transformation and don't have preserve-3d set, wrap 
   * everything in an nsDisplayTransform. If there's nothing in the list, don't add 
   * anything.
   *
   * For the preserve-3d case we want to individually wrap every child in the list with
   * a separate nsDisplayTransform instead. When the child is already an nsDisplayTransform,
   * we can skip this step, as the computed transform will already include our own.
   *
   * We also traverse into sublists created by nsDisplayWrapList or nsDisplayOpacity, so that
   * we find all the correct children.
   */
  if ((mState & NS_FRAME_MAY_BE_TRANSFORMED) &&
      disp->HasTransform() && !resultList.IsEmpty()) {
    if (Preserves3DChildren()) {
      rv = WrapPreserve3DList(this, aBuilder, &resultList);
      if (NS_FAILED(rv))
        return rv;

      if (resultList.Count() > 1) {
        rv = resultList.AppendNewToTop(
          new (aBuilder) nsDisplayWrapList(aBuilder, this, &resultList));
        if (NS_FAILED(rv))
          return rv;
      }
    } else {
      rv = resultList.AppendNewToTop(
        new (aBuilder) nsDisplayTransform(aBuilder, this, &resultList));
      if (NS_FAILED(rv))
        return rv;
    }
  }

  aList->AppendToTop(&resultList);
  return rv;
}

nsresult
nsIFrame::BuildDisplayListForChild(nsDisplayListBuilder*   aBuilder,
                                   nsIFrame*               aChild,
                                   const nsRect&           aDirtyRect,
                                   const nsDisplayListSet& aLists,
                                   PRUint32                aFlags) {
  // If painting is restricted to just the background of the top level frame,
  // then we have nothing to do here.
  if (aBuilder->IsBackgroundOnly())
    return NS_OK;

  if (aChild->GetStateBits() & NS_FRAME_TOO_DEEP_IN_FRAME_TREE)
    return NS_OK;
  
  const nsStyleDisplay* disp = aChild->GetStyleDisplay();
  // PR_TRUE if this is a real or pseudo stacking context
  PRBool pseudoStackingContext =
    (aFlags & DISPLAY_CHILD_FORCE_PSEUDO_STACKING_CONTEXT) != 0;
  // XXX we REALLY need a "are you an inline-block sort of thing?" here!!!
  if ((aFlags & DISPLAY_CHILD_INLINE) &&
      (disp->mDisplay != NS_STYLE_DISPLAY_INLINE ||
       aChild->IsContainingBlock() ||
       (aChild->IsFrameOfType(eReplaced)))) {
    // child is a non-inline frame in an inline context, i.e.,
    // it acts like inline-block or inline-table. Therefore it is a
    // pseudo-stacking-context.
    pseudoStackingContext = PR_TRUE;
  }

  // dirty rect in child-relative coordinates
  nsRect dirty = aDirtyRect - aChild->GetOffsetTo(this);

  nsIAtom* childType = aChild->GetType();
  if (childType == nsGkAtoms::placeholderFrame) {
    nsPlaceholderFrame* placeholder = static_cast<nsPlaceholderFrame*>(aChild);
    aChild = placeholder->GetOutOfFlowFrame();
    NS_ASSERTION(aChild, "No out of flow frame?");
    if (!aChild || nsLayoutUtils::IsPopup(aChild))
      return NS_OK;
    // update for the new child
    disp = aChild->GetStyleDisplay();
    // Make sure that any attempt to use childType below is disappointed. We
    // could call GetType again but since we don't currently need it, let's
    // avoid the virtual call.
    childType = nsnull;
    // Recheck NS_FRAME_TOO_DEEP_IN_FRAME_TREE
    if (aChild->GetStateBits() & NS_FRAME_TOO_DEEP_IN_FRAME_TREE)
      return NS_OK;
    nsRect* savedDirty = static_cast<nsRect*>
      (aChild->Properties().Get(nsDisplayListBuilder::OutOfFlowDirtyRectProperty()));
    if (savedDirty) {
      dirty = *savedDirty;
    } else {
      // The out-of-flow frame did not intersect the dirty area. We may still
      // need to traverse into it, since it may contain placeholders we need
      // to enter to reach other out-of-flow frames that are visible.
      dirty.SetEmpty();
    }
    pseudoStackingContext = PR_TRUE;
  } else if (aBuilder->GetSelectedFramesOnly() &&
             aChild->IsLeaf() &&
             !(aChild->GetStateBits() & NS_FRAME_SELECTED_CONTENT)) {
    return NS_OK;
  }

  if (aBuilder->GetIncludeAllOutOfFlows() &&
      (aChild->GetStateBits() & NS_FRAME_OUT_OF_FLOW)) {
    dirty = aChild->GetVisualOverflowRect();
  } else if (!(aChild->GetStateBits() & NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO)) {
    // No need to descend into aChild to catch placeholders for visible
    // positioned stuff. So see if we can short-circuit frame traversal here.

    // We can stop if aChild's frame subtree's intersection with the
    // dirty area is empty.
    // If the child is a scrollframe that we want to ignore, then we need
    // to descend into it because its scrolled child may intersect the dirty
    // area even if the scrollframe itself doesn't.
    if (aChild != aBuilder->GetIgnoreScrollFrame()) {
      nsRect childDirty;
      if (!childDirty.IntersectRect(dirty, aChild->GetVisualOverflowRect()))
        return NS_OK;
      // Usually we could set dirty to childDirty now but there's no
      // benefit, and it can be confusing. It can especially confuse
      // situations where we're going to ignore a scrollframe's clipping;
      // we wouldn't want to clip the dirty area to the scrollframe's
      // bounds in that case.
    }
  }

  // XXX need to have inline-block and inline-table set pseudoStackingContext
  
  const nsStyleDisplay* ourDisp = GetStyleDisplay();
  // REVIEW: Taken from nsBoxFrame::Paint
  // Don't paint our children if the theme object is a leaf.
  if (IsThemed(ourDisp) &&
      !PresContext()->GetTheme()->WidgetIsContainer(ourDisp->mAppearance))
    return NS_OK;

  // Child is composited if it's transformed, partially transparent, or has
  // SVG effects.
  PRBool isComposited = disp->mOpacity != 1.0f || aChild->IsTransformed()
    || nsSVGIntegrationUtils::UsingEffectsForFrame(aChild);

  PRBool isPositioned = disp->IsPositioned();
  if (isComposited || isPositioned || disp->IsFloating() ||
      (aFlags & DISPLAY_CHILD_FORCE_STACKING_CONTEXT)) {
    // If you change this, also change IsPseudoStackingContextFromStyle()
    pseudoStackingContext = PR_TRUE;
  }
  
  nsRect overflowClip;
  nscoord overflowClipRadii[8];
  PRBool applyOverflowClip =
    ApplyOverflowClipping(aBuilder, aChild, disp, &overflowClip);
  if (applyOverflowClip) {
    aChild->GetPaddingBoxBorderRadii(overflowClipRadii);
  }
  // Don't use overflowClip to restrict the dirty rect, since some of the
  // descendants may not be clipped by it. Even if we end up with unnecessary
  // display items, they'll be pruned during ComputeVisibility. Note that
  // this overflow-clipping here only applies to overflow:-moz-hidden-unscrollable;
  // overflow:hidden etc creates an nsHTML/XULScrollFrame which does its own
  // clipping.

  nsDisplayListBuilder::AutoIsRootSetter rootSetter(aBuilder, pseudoStackingContext);
  nsresult rv;
  if (!pseudoStackingContext) {
    // THIS IS THE COMMON CASE.
    // Not a pseudo or real stacking context. Do the simple thing and
    // return early.
    if (applyOverflowClip) {
      rv = BuildDisplayListWithOverflowClip(aBuilder, aChild, dirty, aLists,
                                            overflowClip, overflowClipRadii);
    } else {
      rv = aChild->BuildDisplayList(aBuilder, dirty, aLists);
      if (NS_SUCCEEDED(rv)) {
        rv = aBuilder->DisplayCaret(aChild, dirty, aLists.Content());
      }
    }
#ifdef NS_DEBUG
    DisplayDebugBorders(aBuilder, aChild, aLists);
#endif
    return rv;
  }
  
  nsDisplayList list;
  nsDisplayList extraPositionedDescendants;
  const nsStylePosition* pos = aChild->GetStylePosition();
  if ((isPositioned && pos->mZIndex.GetUnit() == eStyleUnit_Integer) ||
      isComposited || (aFlags & DISPLAY_CHILD_FORCE_STACKING_CONTEXT)) {
    // True stacking context
    rv = aChild->BuildDisplayListForStackingContext(aBuilder, dirty, &list);
    if (NS_SUCCEEDED(rv)) {
      rv = aBuilder->DisplayCaret(aChild, dirty, &list);
    }
  } else {
    nsRect clipRect;
    PRBool applyAbsPosClipping =
        ApplyAbsPosClipping(aBuilder, disp, aChild, &clipRect);
    // A pseudo-stacking context (e.g., a positioned element with z-index auto).
    // we allow positioned descendants of this element to escape to our
    // container's positioned descendant list, because they might be
    // z-index:non-auto
    nsDisplayListCollection pseudoStack;
    nsRect clippedDirtyRect = dirty;
    if (applyAbsPosClipping) {
      // clipRect is in builder-reference-frame coordinates,
      // dirty/clippedDirtyRect are in aChild coordinates
      clippedDirtyRect.IntersectRect(clippedDirtyRect,
                                     clipRect - aBuilder->ToReferenceFrame(aChild));
    }
    
    if (applyOverflowClip) {
      rv = BuildDisplayListWithOverflowClip(aBuilder, aChild, clippedDirtyRect,
                                            pseudoStack, overflowClip,
                                            overflowClipRadii);
    } else {
      rv = aChild->BuildDisplayList(aBuilder, clippedDirtyRect, pseudoStack);
      if (NS_SUCCEEDED(rv)) {
        rv = aBuilder->DisplayCaret(aChild, dirty, pseudoStack.Content());
      }
    }
    
    if (NS_SUCCEEDED(rv)) {
      if (applyAbsPosClipping) {
        nsAbsPosClipWrapper wrapper(clipRect);
        rv = wrapper.WrapListsInPlace(aBuilder, aChild, pseudoStack);
      }
    }
    list.AppendToTop(pseudoStack.BorderBackground());
    list.AppendToTop(pseudoStack.BlockBorderBackgrounds());
    list.AppendToTop(pseudoStack.Floats());
    list.AppendToTop(pseudoStack.Content());
    list.AppendToTop(pseudoStack.Outlines());
    extraPositionedDescendants.AppendToTop(pseudoStack.PositionedDescendants());
#ifdef NS_DEBUG
    DisplayDebugBorders(aBuilder, aChild, aLists);
#endif
  }
  NS_ENSURE_SUCCESS(rv, rv);
    
  if (isPositioned || isComposited ||
      (aFlags & DISPLAY_CHILD_FORCE_STACKING_CONTEXT)) {
    // Genuine stacking contexts, and positioned pseudo-stacking-contexts,
    // go in this level.
    rv = aLists.PositionedDescendants()->AppendNewToTop(new (aBuilder)
        nsDisplayWrapList(aBuilder, aChild, &list));
    NS_ENSURE_SUCCESS(rv, rv);
  } else if (disp->IsFloating()) {
    rv = aLists.Floats()->AppendNewToTop(new (aBuilder)
        nsDisplayWrapList(aBuilder, aChild, &list));
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    aLists.Content()->AppendToTop(&list);
  }
  // We delay placing the positioned descendants of positioned frames to here,
  // because in the absence of z-index this is the correct order for them.
  // This doesn't affect correctness because the positioned descendants list
  // is sorted by z-order and content in BuildDisplayListForStackingContext,
  // but it means that sort routine needs to do less work.
  aLists.PositionedDescendants()->AppendToTop(&extraPositionedDescendants);
  return NS_OK;
}

void
nsIFrame::WrapReplacedContentForBorderRadius(nsDisplayListBuilder* aBuilder,
                                             nsDisplayList* aFromList,
                                             const nsDisplayListSet& aToLists)
{
  nscoord radii[8];
  if (GetContentBoxBorderRadii(radii)) {
    // If we have a border-radius, we have to clip our content to that
    // radius.
    nsDisplayListCollection set;
    set.Content()->AppendToTop(aFromList);
    nsRect clipRect = GetContentRect() - GetPosition() +
                      aBuilder->ToReferenceFrame(this);
    OverflowClip(aBuilder, set, aToLists, clipRect, radii, PR_FALSE, PR_TRUE);

    return;
  }

  aToLists.Content()->AppendToTop(aFromList);
}

NS_IMETHODIMP  
nsFrame::GetContentForEvent(nsPresContext* aPresContext,
                            nsEvent* aEvent,
                            nsIContent** aContent)
{
  nsIFrame* f = nsLayoutUtils::GetNonGeneratedAncestor(this);
  *aContent = f->GetContent();
  NS_IF_ADDREF(*aContent);
  return NS_OK;
}

void
nsFrame::FireDOMEvent(const nsAString& aDOMEventName, nsIContent *aContent)
{
  nsIContent* target = aContent ? aContent : mContent;

  if (target) {
    nsRefPtr<nsPLDOMEvent> event =
      new nsPLDOMEvent(target, aDOMEventName, PR_TRUE, PR_FALSE);
    if (NS_FAILED(event->PostDOMEvent()))
      NS_WARNING("Failed to dispatch nsPLDOMEvent");
  }
}

NS_IMETHODIMP
nsFrame::HandleEvent(nsPresContext* aPresContext, 
                     nsGUIEvent*     aEvent,
                     nsEventStatus*  aEventStatus)
{

  if (aEvent->message == NS_MOUSE_MOVE) {
    return HandleDrag(aPresContext, aEvent, aEventStatus);
  }

  if (aEvent->eventStructType == NS_MOUSE_EVENT &&
      static_cast<nsMouseEvent*>(aEvent)->button == nsMouseEvent::eLeftButton) {
    if (aEvent->message == NS_MOUSE_BUTTON_DOWN) {
      HandlePress(aPresContext, aEvent, aEventStatus);
    } else if (aEvent->message == NS_MOUSE_BUTTON_UP) {
      HandleRelease(aPresContext, aEvent, aEventStatus);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::GetDataForTableSelection(const nsFrameSelection *aFrameSelection,
                                  nsIPresShell *aPresShell, nsMouseEvent *aMouseEvent, 
                                  nsIContent **aParentContent, PRInt32 *aContentOffset, PRInt32 *aTarget)
{
  if (!aFrameSelection || !aPresShell || !aMouseEvent || !aParentContent || !aContentOffset || !aTarget)
    return NS_ERROR_NULL_POINTER;

  *aParentContent = nsnull;
  *aContentOffset = 0;
  *aTarget = 0;

  PRInt16 displaySelection = aPresShell->GetSelectionFlags();

  PRBool selectingTableCells = aFrameSelection->GetTableCellSelection();

  // DISPLAY_ALL means we're in an editor.
  // If already in cell selection mode, 
  //  continue selecting with mouse drag or end on mouse up,
  //  or when using shift key to extend block of cells
  //  (Mouse down does normal selection unless Ctrl/Cmd is pressed)
  PRBool doTableSelection =
     displaySelection == nsISelectionDisplay::DISPLAY_ALL && selectingTableCells &&
     (aMouseEvent->message == NS_MOUSE_MOVE ||
      (aMouseEvent->message == NS_MOUSE_BUTTON_UP &&
       aMouseEvent->button == nsMouseEvent::eLeftButton) ||
      aMouseEvent->isShift);

  if (!doTableSelection)
  {  
    // In Browser, special 'table selection' key must be pressed for table selection
    // or when just Shift is pressed and we're already in table/cell selection mode
#ifdef XP_MACOSX
    doTableSelection = aMouseEvent->isMeta || (aMouseEvent->isShift && selectingTableCells);
#else
    doTableSelection = aMouseEvent->isControl || (aMouseEvent->isShift && selectingTableCells);
#endif
  }
  if (!doTableSelection) 
    return NS_OK;

  // Get the cell frame or table frame (or parent) of the current content node
  nsIFrame *frame = this;
  PRBool foundCell = PR_FALSE;
  PRBool foundTable = PR_FALSE;

  // Get the limiting node to stop parent frame search
  nsIContent* limiter = aFrameSelection->GetLimiter();

  // If our content node is an ancestor of the limiting node,
  // we should stop the search right now.
  if (limiter && nsContentUtils::ContentIsDescendantOf(limiter, GetContent()))
    return NS_OK;

  //We don't initiate row/col selection from here now,
  //  but we may in future
  //PRBool selectColumn = PR_FALSE;
  //PRBool selectRow = PR_FALSE;
  
  while (frame)
  {
    // Check for a table cell by querying to a known CellFrame interface
    nsITableCellLayout *cellElement = do_QueryFrame(frame);
    if (cellElement)
    {
      foundCell = PR_TRUE;
      //TODO: If we want to use proximity to top or left border
      //      for row and column selection, this is the place to do it
      break;
    }
    else
    {
      // If not a cell, check for table
      // This will happen when starting frame is the table or child of a table,
      //  such as a row (we were inbetween cells or in table border)
      nsITableLayout *tableElement = do_QueryFrame(frame);
      if (tableElement)
      {
        foundTable = PR_TRUE;
        //TODO: How can we select row when along left table edge
        //  or select column when along top edge?
        break;
      } else {
        frame = frame->GetParent();
        // Stop if we have hit the selection's limiting content node
        if (frame && frame->GetContent() == limiter)
          break;
      }
    }
  }
  // We aren't in a cell or table
  if (!foundCell && !foundTable) return NS_OK;

  nsIContent* tableOrCellContent = frame->GetContent();
  if (!tableOrCellContent) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIContent> parentContent = tableOrCellContent->GetParent();
  if (!parentContent) return NS_ERROR_FAILURE;

  PRInt32 offset = parentContent->IndexOf(tableOrCellContent);
  // Not likely?
  if (offset < 0) return NS_ERROR_FAILURE;

  // Everything is OK -- set the return values
  *aParentContent = parentContent;
  NS_ADDREF(*aParentContent);

  *aContentOffset = offset;

#if 0
  if (selectRow)
    *aTarget = nsISelectionPrivate::TABLESELECTION_ROW;
  else if (selectColumn)
    *aTarget = nsISelectionPrivate::TABLESELECTION_COLUMN;
  else 
#endif
  if (foundCell)
    *aTarget = nsISelectionPrivate::TABLESELECTION_CELL;
  else if (foundTable)
    *aTarget = nsISelectionPrivate::TABLESELECTION_TABLE;

  return NS_OK;
}

NS_IMETHODIMP
nsFrame::IsSelectable(PRBool* aSelectable, PRUint8* aSelectStyle) const
{
  if (!aSelectable) //it's ok if aSelectStyle is null
    return NS_ERROR_NULL_POINTER;

  // Like 'visibility', we must check all the parents: if a parent
  // is not selectable, none of its children is selectable.
  //
  // The -moz-all value acts similarly: if a frame has 'user-select:-moz-all',
  // all its children are selectable, even those with 'user-select:none'.
  //
  // As a result, if 'none' and '-moz-all' are not present in the frame hierarchy,
  // aSelectStyle returns the first style that is not AUTO. If these values
  // are present in the frame hierarchy, aSelectStyle returns the style of the
  // topmost parent that has either 'none' or '-moz-all'.
  //
  // For instance, if the frame hierarchy is:
  //    AUTO     -> _MOZ_ALL -> NONE -> TEXT,     the returned value is _MOZ_ALL
  //    TEXT     -> NONE     -> AUTO -> _MOZ_ALL, the returned value is NONE
  //    _MOZ_ALL -> TEXT     -> AUTO -> AUTO,     the returned value is _MOZ_ALL
  //    AUTO     -> CELL     -> TEXT -> AUTO,     the returned value is TEXT
  //
  PRUint8 selectStyle  = NS_STYLE_USER_SELECT_AUTO;
  nsIFrame* frame      = (nsIFrame*)this;

  while (frame) {
    const nsStyleUIReset* userinterface = frame->GetStyleUIReset();
    switch (userinterface->mUserSelect) {
      case NS_STYLE_USER_SELECT_ALL:
      case NS_STYLE_USER_SELECT_NONE:
      case NS_STYLE_USER_SELECT_MOZ_ALL:
        // override the previous values
        selectStyle = userinterface->mUserSelect;
        break;
      default:
        // otherwise return the first value which is not 'auto'
        if (selectStyle == NS_STYLE_USER_SELECT_AUTO) {
          selectStyle = userinterface->mUserSelect;
        }
        break;
    }
    frame = frame->GetParent();
  }

  // convert internal values to standard values
  if (selectStyle == NS_STYLE_USER_SELECT_AUTO)
    selectStyle = NS_STYLE_USER_SELECT_TEXT;
  else
  if (selectStyle == NS_STYLE_USER_SELECT_MOZ_ALL)
    selectStyle = NS_STYLE_USER_SELECT_ALL;
  else
  if (selectStyle == NS_STYLE_USER_SELECT_MOZ_NONE)
    selectStyle = NS_STYLE_USER_SELECT_NONE;

  // return stuff
  if (aSelectable)
    *aSelectable = (selectStyle != NS_STYLE_USER_SELECT_NONE);
  if (aSelectStyle)
    *aSelectStyle = selectStyle;
  if (mState & NS_FRAME_GENERATED_CONTENT)
    *aSelectable = PR_FALSE;
  return NS_OK;
}

/**
  * Handles the Mouse Press Event for the frame
 */
NS_IMETHODIMP
nsFrame::HandlePress(nsPresContext* aPresContext, 
                     nsGUIEvent*     aEvent,
                     nsEventStatus*  aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  if (nsEventStatus_eConsumeNoDefault == *aEventStatus) {
    return NS_OK;
  }

  //We often get out of sync state issues with mousedown events that
  //get interrupted by alerts/dialogs.
  //Check with the ESM to see if we should process this one
  if (!aPresContext->EventStateManager()->EventStatusOK(aEvent)) 
    return NS_OK;

  nsresult rv;
  nsIPresShell *shell = aPresContext->GetPresShell();
  if (!shell)
    return NS_ERROR_FAILURE;

  // if we are in Navigator and the click is in a draggable node, we don't want
  // to start selection because we don't want to interfere with a potential
  // drag of said node and steal all its glory.
  PRInt16 isEditor = shell->GetSelectionFlags();
  //weaaak. only the editor can display frame selection not just text and images
  isEditor = isEditor == nsISelectionDisplay::DISPLAY_ALL;

  nsInputEvent* keyEvent = (nsInputEvent*)aEvent;
  if (!keyEvent->isAlt) {
    
    for (nsIContent* content = mContent; content;
         content = content->GetParent()) {
      if (nsContentUtils::ContentIsDraggable(content) &&
          !content->IsEditable()) {
        // coordinate stuff is the fix for bug #55921
        if ((mRect - GetPosition()).Contains(
               nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, this)))
          return NS_OK;
      }
    }
  }

  // check whether style allows selection
  // if not, don't tell selection the mouse event even occurred.  
  PRBool  selectable;
  PRUint8 selectStyle;
  rv = IsSelectable(&selectable, &selectStyle);
  if (NS_FAILED(rv)) return rv;
  
  // check for select: none
  if (!selectable)
    return NS_OK;

  // When implementing NS_STYLE_USER_SELECT_ELEMENT, NS_STYLE_USER_SELECT_ELEMENTS and
  // NS_STYLE_USER_SELECT_TOGGLE, need to change this logic
  PRBool useFrameSelection = (selectStyle == NS_STYLE_USER_SELECT_TEXT);

  // If the mouse is dragged outside the nearest enclosing scrollable area
  // while making a selection, the area will be scrolled. To do this, capture
  // the mouse on the nearest scrollable frame. If there isn't a scrollable
  // frame, or something else is already capturing the mouse, there's no
  // reason to capture.
  if (!nsIPresShell::GetCapturingContent()) {
    nsIFrame* checkFrame = this;
    nsIScrollableFrame *scrollFrame = nsnull;
    while (checkFrame) {
      scrollFrame = do_QueryFrame(checkFrame);
      if (scrollFrame) {
        nsIPresShell::SetCapturingContent(checkFrame->GetContent(), CAPTURE_IGNOREALLOWED);
        break;
      }
      checkFrame = checkFrame->GetParent();
    }
  }

  // XXX This is screwy; it really should use the selection frame, not the
  // event frame
  const nsFrameSelection* frameselection = nsnull;
  if (useFrameSelection)
    frameselection = GetConstFrameSelection();
  else
    frameselection = shell->ConstFrameSelection();

  if (!frameselection || frameselection->GetDisplaySelection() == nsISelectionController::SELECTION_OFF)
    return NS_OK;//nothing to do we cannot affect selection from here

  nsMouseEvent *me = (nsMouseEvent *)aEvent;

#ifdef XP_MACOSX
  if (me->isControl)
    return NS_OK;//short ciruit. hard coded for mac due to time restraints.
  PRBool control = me->isMeta;
#else
  PRBool control = me->isControl;
#endif

  nsRefPtr<nsFrameSelection> fc = const_cast<nsFrameSelection*>(frameselection);
  if (me->clickCount >1 )
  {
    // These methods aren't const but can't actually delete anything,
    // so no need for nsWeakFrame.
    fc->SetMouseDownState(PR_TRUE);
    fc->SetMouseDoubleDown(PR_TRUE);
    return HandleMultiplePress(aPresContext, aEvent, aEventStatus, control);
  }

  nsPoint pt = nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, this);
  ContentOffsets offsets = GetContentOffsetsFromPoint(pt);

  if (!offsets.content)
    return NS_ERROR_FAILURE;

  // Let Ctrl/Cmd+mouse down do table selection instead of drag initiation
  nsCOMPtr<nsIContent>parentContent;
  PRInt32  contentOffset;
  PRInt32 target;
  rv = GetDataForTableSelection(frameselection, shell, me, getter_AddRefs(parentContent), &contentOffset, &target);
  if (NS_SUCCEEDED(rv) && parentContent)
  {
    fc->SetMouseDownState(PR_TRUE);
    return fc->HandleTableSelection(parentContent, contentOffset, target, me);
  }

  fc->SetDelayedCaretData(0);

  // Check if any part of this frame is selected, and if the
  // user clicked inside the selected region. If so, we delay
  // starting a new selection since the user may be trying to
  // drag the selected region to some other app.

  SelectionDetails *details = 0;
  PRBool isSelected = ((GetStateBits() & NS_FRAME_SELECTED_CONTENT) == NS_FRAME_SELECTED_CONTENT);

  if (isSelected)
  {
    PRBool inSelection = PR_FALSE;
    details = frameselection->LookUpSelection(offsets.content, 0,
        offsets.EndOffset(), PR_FALSE);

    //
    // If there are any details, check to see if the user clicked
    // within any selected region of the frame.
    //

    SelectionDetails *curDetail = details;

    while (curDetail)
    {
      //
      // If the user clicked inside a selection, then just
      // return without doing anything. We will handle placing
      // the caret later on when the mouse is released. We ignore
      // the spellcheck, find and url formatting selections.
      //
      if (curDetail->mType != nsISelectionController::SELECTION_SPELLCHECK &&
          curDetail->mType != nsISelectionController::SELECTION_FIND &&
          curDetail->mType != nsISelectionController::SELECTION_URLSECONDARY &&
          curDetail->mStart <= offsets.StartOffset() &&
          offsets.EndOffset() <= curDetail->mEnd)
      {
        inSelection = PR_TRUE;
      }

      SelectionDetails *nextDetail = curDetail->mNext;
      delete curDetail;
      curDetail = nextDetail;
    }

    if (inSelection) {
      fc->SetMouseDownState(PR_FALSE);
      fc->SetDelayedCaretData(me);
      return NS_OK;
    }
  }

  fc->SetMouseDownState(PR_TRUE);

  // Do not touch any nsFrame members after this point without adding
  // weakFrame checks.
  rv = fc->HandleClick(offsets.content, offsets.StartOffset(),
                       offsets.EndOffset(), me->isShift, control,
                       offsets.associateWithNext);

  if (NS_FAILED(rv))
    return rv;

  if (offsets.offset != offsets.secondaryOffset)
    fc->MaintainSelection();

  if (isEditor && !me->isShift &&
      (offsets.EndOffset() - offsets.StartOffset()) == 1)
  {
    // A single node is selected and we aren't extending an existing
    // selection, which means the user clicked directly on an object (either
    // -moz-user-select: all or a non-text node without children).
    // Therefore, disable selection extension during mouse moves.
    // XXX This is a bit hacky; shouldn't editor be able to deal with this?
    fc->SetMouseDownState(PR_FALSE);
  }

  return rv;
}

/**
  * Multiple Mouse Press -- line or paragraph selection -- for the frame.
  * Wouldn't it be nice if this didn't have to be hardwired into Frame code?
 */
NS_IMETHODIMP
nsFrame::HandleMultiplePress(nsPresContext* aPresContext, 
                             nsGUIEvent*    aEvent,
                             nsEventStatus* aEventStatus,
                             PRBool         aControlHeld)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  if (nsEventStatus_eConsumeNoDefault == *aEventStatus) {
    return NS_OK;
  }

  if (DisplaySelection(aPresContext) == nsISelectionController::SELECTION_OFF) {
    return NS_OK;
  }

  // Find out whether we're doing line or paragraph selection.
  // If browser.triple_click_selects_paragraph is true, triple-click selects paragraph.
  // Otherwise, triple-click selects line, and quadruple-click selects paragraph
  // (on platforms that support quadruple-click).
  nsSelectionAmount beginAmount, endAmount;
  nsMouseEvent *me = (nsMouseEvent *)aEvent;
  if (!me) return NS_OK;

  if (me->clickCount == 4) {
    beginAmount = endAmount = eSelectParagraph;
  } else if (me->clickCount == 3) {
    if (Preferences::GetBool("browser.triple_click_selects_paragraph")) {
      beginAmount = endAmount = eSelectParagraph;
    } else {
      beginAmount = eSelectBeginLine;
      endAmount = eSelectEndLine;
    }
  } else if (me->clickCount == 2) {
    // We only want inline frames; PeekBackwardAndForward dislikes blocks
    beginAmount = endAmount = eSelectWord;
  } else {
    return NS_OK;
  }

  nsPoint pt = nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, this);
  ContentOffsets offsets = GetContentOffsetsFromPoint(pt);
  if (!offsets.content) return NS_ERROR_FAILURE;

  nsIFrame* theFrame;
  PRInt32 offset;
  // Maybe make this a static helper?
  const nsFrameSelection* frameSelection =
    PresContext()->GetPresShell()->ConstFrameSelection();
  theFrame = frameSelection->
    GetFrameForNodeOffset(offsets.content, offsets.offset,
                          nsFrameSelection::HINT(offsets.associateWithNext),
                          &offset);
  if (!theFrame)
    return NS_ERROR_FAILURE;

  nsFrame* frame = static_cast<nsFrame*>(theFrame);

  return frame->PeekBackwardAndForward(beginAmount, endAmount,
                                       offsets.offset, aPresContext,
                                       beginAmount != eSelectWord,
                                       aControlHeld);
}

NS_IMETHODIMP
nsFrame::PeekBackwardAndForward(nsSelectionAmount aAmountBack,
                                nsSelectionAmount aAmountForward,
                                PRInt32 aStartPos,
                                nsPresContext* aPresContext,
                                PRBool aJumpLines,
                                PRBool aMultipleSelection)
{
  nsIFrame* baseFrame = this;
  PRInt32 baseOffset = aStartPos;
  nsresult rv;

  if (aAmountBack == eSelectWord) {
    // To avoid selecting the previous word when at start of word,
    // first move one character forward.
    nsPeekOffsetStruct pos;
    pos.SetData(eSelectCharacter,
                eDirNext,
                aStartPos,
                0,
                aJumpLines,
                PR_TRUE,  //limit on scrolled views
                PR_FALSE,
                PR_FALSE);
    rv = PeekOffset(&pos);
    if (NS_SUCCEEDED(rv)) {
      baseFrame = pos.mResultFrame;
      baseOffset = pos.mContentOffset;
    }
  }

  // Use peek offset one way then the other:  
  nsPeekOffsetStruct startpos;
  startpos.SetData(aAmountBack,
                   eDirPrevious,
                   baseOffset, 
                   0,
                   aJumpLines,
                   PR_TRUE,  //limit on scrolled views
                   PR_FALSE,
                   PR_FALSE);
  rv = baseFrame->PeekOffset(&startpos);
  if (NS_FAILED(rv))
    return rv;

  nsPeekOffsetStruct endpos;
  endpos.SetData(aAmountForward,
                 eDirNext,
                 aStartPos, 
                 0,
                 aJumpLines,
                 PR_TRUE,  //limit on scrolled views
                 PR_FALSE,
                 PR_FALSE);
  rv = PeekOffset(&endpos);
  if (NS_FAILED(rv))
    return rv;

  // Keep frameSelection alive.
  nsRefPtr<nsFrameSelection> frameSelection = GetFrameSelection();

  rv = frameSelection->HandleClick(startpos.mResultContent,
                                   startpos.mContentOffset, startpos.mContentOffset,
                                   PR_FALSE, aMultipleSelection,
                                   nsFrameSelection::HINTRIGHT);
  if (NS_FAILED(rv))
    return rv;

  rv = frameSelection->HandleClick(endpos.mResultContent,
                                   endpos.mContentOffset, endpos.mContentOffset,
                                   PR_TRUE, PR_FALSE,
                                   nsFrameSelection::HINTLEFT);
  if (NS_FAILED(rv))
    return rv;

  // maintain selection
  return frameSelection->MaintainSelection(aAmountBack);
}

NS_IMETHODIMP nsFrame::HandleDrag(nsPresContext* aPresContext, 
                                  nsGUIEvent*     aEvent,
                                  nsEventStatus*  aEventStatus)
{
  PRBool  selectable;
  PRUint8 selectStyle;
  IsSelectable(&selectable, &selectStyle);
  // XXX Do we really need to exclude non-selectable content here?
  // GetContentOffsetsFromPoint can handle it just fine, although some
  // other stuff might not like it.
  if (!selectable)
    return NS_OK;
  if (DisplaySelection(aPresContext) == nsISelectionController::SELECTION_OFF) {
    return NS_OK;
  }
  nsIPresShell *presShell = aPresContext->PresShell();

  nsRefPtr<nsFrameSelection> frameselection = GetFrameSelection();
  PRBool mouseDown = frameselection->GetMouseDownState();
  if (!mouseDown)
    return NS_OK;

  frameselection->StopAutoScrollTimer();

  // Check if we are dragging in a table cell
  nsCOMPtr<nsIContent> parentContent;
  PRInt32 contentOffset;
  PRInt32 target;
  nsMouseEvent *me = (nsMouseEvent *)aEvent;
  nsresult result;
  result = GetDataForTableSelection(frameselection, presShell, me,
                                    getter_AddRefs(parentContent),
                                    &contentOffset, &target);      

  nsWeakFrame weakThis = this;
  if (NS_SUCCEEDED(result) && parentContent) {
    frameselection->HandleTableSelection(parentContent, contentOffset, target, me);
  } else {
    nsPoint pt = nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, this);
    frameselection->HandleDrag(this, pt);
  }

  // The frameselection object notifies selection listeners synchronously above
  // which might have killed us.
  if (!weakThis.IsAlive()) {
    return NS_OK;
  }

  // get the nearest scrollframe
  nsIFrame* checkFrame = this;
  nsIScrollableFrame *scrollFrame = nsnull;
  while (checkFrame) {
    scrollFrame = do_QueryFrame(checkFrame);
    if (scrollFrame) {
      break;
    }
    checkFrame = checkFrame->GetParent();
  }

  if (scrollFrame) {
    nsIFrame* capturingFrame = scrollFrame->GetScrolledFrame();
    if (capturingFrame) {
      nsPoint pt =
        nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, capturingFrame);
      frameselection->StartAutoScrollTimer(capturingFrame, pt, 30);
    }
  }

  return NS_OK;
}

/**
 * This static method handles part of the nsFrame::HandleRelease in a way
 * which doesn't rely on the nsFrame object to stay alive.
 */
static nsresult
HandleFrameSelection(nsFrameSelection*         aFrameSelection,
                     nsIFrame::ContentOffsets& aOffsets,
                     PRBool                    aHandleTableSel,
                     PRInt32                   aContentOffsetForTableSel,
                     PRInt32                   aTargetForTableSel,
                     nsIContent*               aParentContentForTableSel,
                     nsGUIEvent*               aEvent,
                     nsEventStatus*            aEventStatus)
{
  if (!aFrameSelection) {
    return NS_OK;
  }

  nsresult rv = NS_OK;

  if (nsEventStatus_eConsumeNoDefault != *aEventStatus) {
    if (!aHandleTableSel) {
      nsMouseEvent *me = aFrameSelection->GetDelayedCaretData();
      if (!aOffsets.content || !me) {
        return NS_ERROR_FAILURE;
      }

      // We are doing this to simulate what we would have done on HandlePress.
      // We didn't do it there to give the user an opportunity to drag
      // the text, but since they didn't drag, we want to place the
      // caret.
      // However, we'll use the mouse position from the release, since:
      //  * it's easier
      //  * that's the normal click position to use (although really, in
      //    the normal case, small movements that don't count as a drag
      //    can do selection)
      aFrameSelection->SetMouseDownState(PR_TRUE);

      rv = aFrameSelection->HandleClick(aOffsets.content,
                                        aOffsets.StartOffset(),
                                        aOffsets.EndOffset(),
                                        me->isShift, PR_FALSE,
                                        aOffsets.associateWithNext);
      if (NS_FAILED(rv)) {
        return rv;
      }
    } else if (aParentContentForTableSel) {
      aFrameSelection->SetMouseDownState(PR_FALSE);
      rv = aFrameSelection->HandleTableSelection(aParentContentForTableSel,
                                                 aContentOffsetForTableSel,
                                                 aTargetForTableSel,
                                                 (nsMouseEvent *)aEvent);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
    aFrameSelection->SetDelayedCaretData(0);
  }

  aFrameSelection->SetMouseDownState(PR_FALSE);
  aFrameSelection->StopAutoScrollTimer();

  return NS_OK;
}

NS_IMETHODIMP nsFrame::HandleRelease(nsPresContext* aPresContext,
                                     nsGUIEvent*    aEvent,
                                     nsEventStatus* aEventStatus)
{
  nsIFrame* activeFrame = GetActiveSelectionFrame(aPresContext, this);

  nsCOMPtr<nsIContent> captureContent = nsIPresShell::GetCapturingContent();

  // We can unconditionally stop capturing because
  // we should never be capturing when the mouse button is up
  nsIPresShell::SetCapturingContent(nsnull, 0);

  PRBool selectionOff =
    (DisplaySelection(aPresContext) == nsISelectionController::SELECTION_OFF);

  nsRefPtr<nsFrameSelection> frameselection;
  ContentOffsets offsets;
  nsCOMPtr<nsIContent> parentContent;
  PRInt32 contentOffsetForTableSel = 0;
  PRInt32 targetForTableSel = 0;
  PRBool handleTableSelection = PR_TRUE;

  if (!selectionOff) {
    frameselection = GetFrameSelection();
    if (nsEventStatus_eConsumeNoDefault != *aEventStatus && frameselection) {
      // Check if the frameselection recorded the mouse going down.
      // If not, the user must have clicked in a part of the selection.
      // Place the caret before continuing!

      PRBool mouseDown = frameselection->GetMouseDownState();
      nsMouseEvent *me = frameselection->GetDelayedCaretData();

      if (!mouseDown && me && me->clickCount < 2) {
        nsPoint pt = nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, this);
        offsets = GetContentOffsetsFromPoint(pt);
        handleTableSelection = PR_FALSE;
      } else {
        GetDataForTableSelection(frameselection, PresContext()->PresShell(),
                                 (nsMouseEvent *)aEvent,
                                 getter_AddRefs(parentContent),
                                 &contentOffsetForTableSel,
                                 &targetForTableSel);
      }
    }
  }

  // We might be capturing in some other document and the event just happened to
  // trickle down here. Make sure that document's frame selection is notified.
  // Note, this may cause the current nsFrame object to be deleted, bug 336592.
  nsRefPtr<nsFrameSelection> frameSelection;
  if (activeFrame != this &&
      static_cast<nsFrame*>(activeFrame)->DisplaySelection(activeFrame->PresContext())
        != nsISelectionController::SELECTION_OFF) {
      frameSelection = activeFrame->GetFrameSelection();
  }

  // Also check the selection of the capturing content which might be in a
  // different document.
  if (!frameSelection && captureContent) {
    nsIDocument* doc = captureContent->GetCurrentDoc();
    if (doc) {
      nsIPresShell* capturingShell = doc->GetShell();
      if (capturingShell && capturingShell != PresContext()->GetPresShell()) {
        frameSelection = capturingShell->FrameSelection();
      }
    }
  }

  if (frameSelection) {
    frameSelection->SetMouseDownState(PR_FALSE);
    frameSelection->StopAutoScrollTimer();
  }

  // Do not call any methods of the current object after this point!!!
  // The object is perhaps dead!

  return selectionOff
    ? NS_OK
    : HandleFrameSelection(frameselection, offsets, handleTableSelection,
                           contentOffsetForTableSel, targetForTableSel,
                           parentContent, aEvent, aEventStatus);
}

struct NS_STACK_CLASS FrameContentRange {
  FrameContentRange(nsIContent* aContent, PRInt32 aStart, PRInt32 aEnd) :
    content(aContent), start(aStart), end(aEnd) { }
  nsCOMPtr<nsIContent> content;
  PRInt32 start;
  PRInt32 end;
};

// Retrieve the content offsets of a frame
static FrameContentRange GetRangeForFrame(nsIFrame* aFrame) {
  nsCOMPtr<nsIContent> content, parent;
  content = aFrame->GetContent();
  if (!content) {
    NS_WARNING("Frame has no content");
    return FrameContentRange(nsnull, -1, -1);
  }
  nsIAtom* type = aFrame->GetType();
  if (type == nsGkAtoms::textFrame) {
    PRInt32 offset, offsetEnd;
    aFrame->GetOffsets(offset, offsetEnd);
    return FrameContentRange(content, offset, offsetEnd);
  }
  if (type == nsGkAtoms::brFrame) {
    parent = content->GetParent();
    PRInt32 beginOffset = parent->IndexOf(content);
    return FrameContentRange(parent, beginOffset, beginOffset);
  }
  // Loop to deal with anonymous content, which has no index; this loop
  // probably won't run more than twice under normal conditions
  do {
    parent  = content->GetParent();
    if (parent) {
      PRInt32 beginOffset = parent->IndexOf(content);
      if (beginOffset >= 0)
        return FrameContentRange(parent, beginOffset, beginOffset + 1);
      content = parent;
    }
  } while (parent);

  // The root content node must act differently
  return FrameContentRange(content, 0, content->GetChildCount());
}

// The FrameTarget represents the closest frame to a point that can be selected
// The frame is the frame represented, frameEdge says whether one end of the
// frame is the result (in which case different handling is needed), and
// afterFrame says which end is repersented if frameEdge is true
struct FrameTarget {
  FrameTarget(nsIFrame* aFrame, PRBool aFrameEdge, PRBool aAfterFrame,
              PRBool aEmptyBlock = PR_FALSE) :
    frame(aFrame), frameEdge(aFrameEdge), afterFrame(aAfterFrame),
    emptyBlock(aEmptyBlock) { }
  static FrameTarget Null() {
    return FrameTarget(nsnull, PR_FALSE, PR_FALSE);
  }
  PRBool IsNull() {
    return !frame;
  }
  nsIFrame* frame;
  PRPackedBool frameEdge;
  PRPackedBool afterFrame;
  PRPackedBool emptyBlock;
};

// See function implementation for information
static FrameTarget GetSelectionClosestFrame(nsIFrame* aFrame, nsPoint aPoint);

static PRBool SelfIsSelectable(nsIFrame* aFrame)
{
  return !(aFrame->IsGeneratedContentFrame() ||
           aFrame->GetStyleUIReset()->mUserSelect == NS_STYLE_USER_SELECT_NONE);
}

static PRBool SelectionDescendToKids(nsIFrame* aFrame) {
  PRUint8 style = aFrame->GetStyleUIReset()->mUserSelect;
  nsIFrame* parent = aFrame->GetParent();
  // If we are only near (not directly over) then don't traverse
  // frames with independent selection (e.g. text and list controls)
  // unless we're already inside such a frame (see bug 268497).  Note that this
  // prevents any of the users of this method from entering form controls.
  // XXX We might want some way to allow using the up-arrow to go into a form
  // control, but the focus didn't work right anyway; it'd probably be enough
  // if the left and right arrows could enter textboxes (which I don't believe
  // they can at the moment)
  return !aFrame->IsGeneratedContentFrame() &&
         style != NS_STYLE_USER_SELECT_ALL  &&
         style != NS_STYLE_USER_SELECT_NONE &&
         ((parent->GetStateBits() & NS_FRAME_INDEPENDENT_SELECTION) ||
          !(aFrame->GetStateBits() & NS_FRAME_INDEPENDENT_SELECTION));
}

static FrameTarget GetSelectionClosestFrameForChild(nsIFrame* aChild,
                                                    nsPoint aPoint)
{
  nsIFrame* parent = aChild->GetParent();
  if (SelectionDescendToKids(aChild)) {
    nsPoint pt = aPoint - aChild->GetOffsetTo(parent);
    return GetSelectionClosestFrame(aChild, pt);
  }
  return FrameTarget(aChild, PR_FALSE, PR_FALSE);
}

// When the cursor needs to be at the beginning of a block, it shouldn't be
// before the first child.  A click on a block whose first child is a block
// should put the cursor in the child.  The cursor shouldn't be between the
// blocks, because that's not where it's expected.
// Note that this method is guaranteed to succeed.
static FrameTarget DrillDownToSelectionFrame(nsIFrame* aFrame,
                                             PRBool aEndFrame) {
  if (SelectionDescendToKids(aFrame)) {
    nsIFrame* result = nsnull;
    nsIFrame *frame = aFrame->GetFirstPrincipalChild();
    if (!aEndFrame) {
      while (frame && (!SelfIsSelectable(frame) ||
                        frame->IsEmpty()))
        frame = frame->GetNextSibling();
      if (frame)
        result = frame;
    } else {
      // Because the frame tree is singly linked, to find the last frame,
      // we have to iterate through all the frames
      // XXX I have a feeling this could be slow for long blocks, although
      //     I can't find any slowdowns
      while (frame) {
        if (!frame->IsEmpty() && SelfIsSelectable(frame))
          result = frame;
        frame = frame->GetNextSibling();
      }
    }
    if (result)
      return DrillDownToSelectionFrame(result, aEndFrame);
  }
  // If the current frame has no targetable children, target the current frame
  return FrameTarget(aFrame, PR_TRUE, aEndFrame);
}

// This method finds the closest valid FrameTarget on a given line; if there is
// no valid FrameTarget on the line, it returns a null FrameTarget
static FrameTarget GetSelectionClosestFrameForLine(
                      nsBlockFrame* aParent,
                      nsBlockFrame::line_iterator aLine,
                      nsPoint aPoint)
{
  nsIFrame *frame = aLine->mFirstChild;
  // Account for end of lines (any iterator from the block is valid)
  if (aLine == aParent->end_lines())
    return DrillDownToSelectionFrame(aParent, PR_TRUE);
  nsIFrame *closestFromLeft = nsnull, *closestFromRight = nsnull;
  nsRect rect = aLine->mBounds;
  nscoord closestLeft = rect.x, closestRight = rect.XMost();
  for (PRInt32 n = aLine->GetChildCount(); n;
       --n, frame = frame->GetNextSibling()) {
    if (!SelfIsSelectable(frame) || frame->IsEmpty())
      continue;
    nsRect frameRect = frame->GetRect();
    if (aPoint.x >= frameRect.x) {
      if (aPoint.x < frameRect.XMost()) {
        return GetSelectionClosestFrameForChild(frame, aPoint);
      }
      if (frameRect.XMost() >= closestLeft) {
        closestFromLeft = frame;
        closestLeft = frameRect.XMost();
      }
    } else {
      if (frameRect.x <= closestRight) {
        closestFromRight = frame;
        closestRight = frameRect.x;
      }
    }
  }
  if (!closestFromLeft && !closestFromRight) {
    // We should only get here if there are no selectable frames on a line
    // XXX Do we need more elaborate handling here?
    return FrameTarget::Null();
  }
  if (closestFromLeft &&
      (!closestFromRight ||
       (abs(aPoint.x - closestLeft) <= abs(aPoint.x - closestRight)))) {
    return GetSelectionClosestFrameForChild(closestFromLeft, aPoint);
  }
  return GetSelectionClosestFrameForChild(closestFromRight, aPoint);
}

// This method is for the special handling we do for block frames; they're
// special because they represent paragraphs and because they are organized
// into lines, which have bounds that are not stored elsewhere in the
// frame tree.  Returns a null FrameTarget for frames which are not
// blocks or blocks with no lines except editable one.
static FrameTarget GetSelectionClosestFrameForBlock(nsIFrame* aFrame,
                                                    nsPoint aPoint)
{
  nsBlockFrame* bf = nsLayoutUtils::GetAsBlock(aFrame); // used only for QI
  if (!bf)
    return FrameTarget::Null();

  // This code searches for the correct line
  nsBlockFrame::line_iterator firstLine = bf->begin_lines();
  nsBlockFrame::line_iterator end = bf->end_lines();
  if (firstLine == end) {
    nsIContent *blockContent = aFrame->GetContent();
    if (blockContent && blockContent->IsEditable()) {
      // If the frame is ediable empty block, we should return it with empty
      // flag.
      return FrameTarget(aFrame, PR_FALSE, PR_FALSE, PR_TRUE);
    }
    return FrameTarget::Null();
  }
  nsBlockFrame::line_iterator curLine = firstLine;
  nsBlockFrame::line_iterator closestLine = end;
  while (curLine != end) {
    // Check to see if our point lies with the line's Y bounds
    nscoord y = aPoint.y - curLine->mBounds.y;
    nscoord height = curLine->mBounds.height;
    if (y >= 0 && y < height) {
      closestLine = curLine;
      break; // We found the line; stop looking
    }
    if (y < 0)
      break;
    ++curLine;
  }

  if (closestLine == end) {
    nsBlockFrame::line_iterator prevLine = curLine.prev();
    nsBlockFrame::line_iterator nextLine = curLine;
    // Avoid empty lines
    while (nextLine != end && nextLine->IsEmpty())
      ++nextLine;
    while (prevLine != end && prevLine->IsEmpty())
      --prevLine;

    // This hidden pref dictates whether a point above or below all lines comes
    // up with a line or the beginning or end of the frame; 0 on Windows,
    // 1 on other platforms by default at the writing of this code
    PRInt32 dragOutOfFrame =
      Preferences::GetInt("browser.drag_out_of_frame_style");

    if (prevLine == end) {
      if (dragOutOfFrame == 1 || nextLine == end)
        return DrillDownToSelectionFrame(aFrame, PR_FALSE);
      closestLine = nextLine;
    } else if (nextLine == end) {
      if (dragOutOfFrame == 1)
        return DrillDownToSelectionFrame(aFrame, PR_TRUE);
      closestLine = prevLine;
    } else { // Figure out which line is closer
      if (aPoint.y - prevLine->mBounds.YMost() < nextLine->mBounds.y - aPoint.y)
        closestLine = prevLine;
      else
        closestLine = nextLine;
    }
  }

  do {
    FrameTarget target = GetSelectionClosestFrameForLine(bf, closestLine,
                                                         aPoint);
    if (!target.IsNull())
      return target;
    ++closestLine;
  } while (closestLine != end);
  // Fall back to just targeting the last targetable place
  return DrillDownToSelectionFrame(aFrame, PR_TRUE);
}

// GetSelectionClosestFrame is the helper function that calculates the closest
// frame to the given point.
// It doesn't completely account for offset styles, so needs to be used in
// restricted environments.
// Cannot handle overlapping frames correctly, so it should receive the output
// of GetFrameForPoint
// Guaranteed to return a valid FrameTarget
static FrameTarget GetSelectionClosestFrame(nsIFrame* aFrame, nsPoint aPoint)
{
  {
    // Handle blocks; if the frame isn't a block, the method fails
    FrameTarget target = GetSelectionClosestFrameForBlock(aFrame, aPoint);
    if (!target.IsNull())
      return target;
  }

  nsIFrame *kid = aFrame->GetFirstPrincipalChild();

  if (kid) {
    // Go through all the child frames to find the closest one

    // Large number to force the comparison to succeed
    const nscoord HUGE_DISTANCE = nscoord_MAX;
    nscoord closestXDistance = HUGE_DISTANCE;
    nscoord closestYDistance = HUGE_DISTANCE;
    nsIFrame *closestFrame = nsnull;

    for (; kid; kid = kid->GetNextSibling()) {
      if (!SelfIsSelectable(kid) || kid->IsEmpty())
        continue;

      nsRect rect = kid->GetRect();

      nscoord fromLeft = aPoint.x - rect.x;
      nscoord fromRight = aPoint.x - rect.XMost();

      nscoord xDistance;
      if (fromLeft >= 0 && fromRight <= 0) {
        xDistance = 0;
      } else {
        xDistance = NS_MIN(abs(fromLeft), abs(fromRight));
      }

      if (xDistance <= closestXDistance)
      {
        if (xDistance < closestXDistance)
          closestYDistance = HUGE_DISTANCE;

        nscoord fromTop = aPoint.y - rect.y;
        nscoord fromBottom = aPoint.y - rect.YMost();

        nscoord yDistance;
        if (fromTop >= 0 && fromBottom <= 0)
          yDistance = 0;
        else
          yDistance = NS_MIN(abs(fromTop), abs(fromBottom));

        if (yDistance < closestYDistance)
        {
          closestXDistance = xDistance;
          closestYDistance = yDistance;
          closestFrame = kid;
        }
      }
    }
    if (closestFrame)
      return GetSelectionClosestFrameForChild(closestFrame, aPoint);
  }
  return FrameTarget(aFrame, PR_FALSE, PR_FALSE);
}

nsIFrame::ContentOffsets OffsetsForSingleFrame(nsIFrame* aFrame, nsPoint aPoint)
{
  nsIFrame::ContentOffsets offsets;
  FrameContentRange range = GetRangeForFrame(aFrame);
  offsets.content = range.content;
  // If there are continuations (meaning it's not one rectangle), this is the
  // best this function can do
  if (aFrame->GetNextContinuation() || aFrame->GetPrevContinuation()) {
    offsets.offset = range.start;
    offsets.secondaryOffset = range.end;
    offsets.associateWithNext = PR_TRUE;
    return offsets;
  }

  // Figure out whether the offsets should be over, after, or before the frame
  nsRect rect(nsPoint(0, 0), aFrame->GetSize());

  PRBool isBlock = (aFrame->GetStyleDisplay()->mDisplay != NS_STYLE_DISPLAY_INLINE);
  PRBool isRtl = (aFrame->GetStyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL);
  if ((isBlock && rect.y < aPoint.y) ||
      (!isBlock && ((isRtl  && rect.x + rect.width / 2 > aPoint.x) || 
                    (!isRtl && rect.x + rect.width / 2 < aPoint.x)))) {
    offsets.offset = range.end;
    if (rect.Contains(aPoint))
      offsets.secondaryOffset = range.start;
    else
      offsets.secondaryOffset = range.end;
  } else {
    offsets.offset = range.start;
    if (rect.Contains(aPoint))
      offsets.secondaryOffset = range.end;
    else
      offsets.secondaryOffset = range.start;
  }
  offsets.associateWithNext = (offsets.offset == range.start);
  return offsets;
}

static nsIFrame* AdjustFrameForSelectionStyles(nsIFrame* aFrame) {
  nsIFrame* adjustedFrame = aFrame;
  for (nsIFrame* frame = aFrame; frame; frame = frame->GetParent())
  {
    // These are the conditions that make all children not able to handle
    // a cursor.
    if (frame->GetStyleUIReset()->mUserSelect == NS_STYLE_USER_SELECT_NONE || 
        frame->GetStyleUIReset()->mUserSelect == NS_STYLE_USER_SELECT_ALL || 
        frame->IsGeneratedContentFrame()) {
      adjustedFrame = frame;
    }
  }
  return adjustedFrame;
}
  

nsIFrame::ContentOffsets nsIFrame::GetContentOffsetsFromPoint(nsPoint aPoint,
                                                              PRBool aIgnoreSelectionStyle)
{
  nsIFrame *adjustedFrame;
  if (aIgnoreSelectionStyle) {
    adjustedFrame = this;
  }
  else {
    // This section of code deals with special selection styles.  Note that
    // -moz-none and -moz-all exist, even though they don't need to be explicitly
    // handled.
    // The offset is forced not to end up in generated content; content offsets
    // cannot represent content outside of the document's content tree.

    adjustedFrame = AdjustFrameForSelectionStyles(this);

    // -moz-user-select: all needs special handling, because clicking on it
    // should lead to the whole frame being selected
    if (adjustedFrame && adjustedFrame->GetStyleUIReset()->mUserSelect ==
        NS_STYLE_USER_SELECT_ALL) {
      return OffsetsForSingleFrame(adjustedFrame, aPoint +
                                   this->GetOffsetTo(adjustedFrame));
    }

    // For other cases, try to find a closest frame starting from the parent of
    // the unselectable frame
    if (adjustedFrame != this)
      adjustedFrame = adjustedFrame->GetParent();
  }

  nsPoint adjustedPoint = aPoint + this->GetOffsetTo(adjustedFrame);

  FrameTarget closest = GetSelectionClosestFrame(adjustedFrame, adjustedPoint);

  if (closest.emptyBlock) {
    ContentOffsets offsets;
    NS_ASSERTION(closest.frame,
                 "closest.frame must not be null when it's empty");
    offsets.content = closest.frame->GetContent();
    offsets.offset = 0;
    offsets.secondaryOffset = 0;
    offsets.associateWithNext = PR_TRUE;
    return offsets;
  }

  // If the correct offset is at one end of a frame, use offset-based
  // calculation method
  if (closest.frameEdge) {
    ContentOffsets offsets;
    FrameContentRange range = GetRangeForFrame(closest.frame);
    offsets.content = range.content;
    if (closest.afterFrame)
      offsets.offset = range.end;
    else
      offsets.offset = range.start;
    offsets.secondaryOffset = offsets.offset;
    offsets.associateWithNext = (offsets.offset == range.start);
    return offsets;
  }
  nsPoint pt = aPoint - closest.frame->GetOffsetTo(this);
  return static_cast<nsFrame*>(closest.frame)->CalcContentOffsetsFromFramePoint(pt);

  // XXX should I add some kind of offset standardization?
  // consider <b>xxxxx</b><i>zzzzz</i>; should any click between the last
  // x and first z put the cursor in the same logical position in addition
  // to the same visual position?
}

nsIFrame::ContentOffsets nsFrame::CalcContentOffsetsFromFramePoint(nsPoint aPoint)
{
  return OffsetsForSingleFrame(this, aPoint);
}

NS_IMETHODIMP
nsFrame::GetCursor(const nsPoint& aPoint,
                   nsIFrame::Cursor& aCursor)
{
  FillCursorInformationFromStyle(GetStyleUserInterface(), aCursor);
  if (NS_STYLE_CURSOR_AUTO == aCursor.mCursor) {
    aCursor.mCursor = NS_STYLE_CURSOR_DEFAULT;
  }


  return NS_OK;
}

// Resize and incremental reflow

/* virtual */ void
nsFrame::MarkIntrinsicWidthsDirty()
{
  // This version is meant only for what used to be box-to-block adaptors.
  // It should not be called by other derived classes.
  if (IsBoxWrapped()) {
    nsBoxLayoutMetrics *metrics = BoxMetrics();

    SizeNeedsRecalc(metrics->mPrefSize);
    SizeNeedsRecalc(metrics->mMinSize);
    SizeNeedsRecalc(metrics->mMaxSize);
    SizeNeedsRecalc(metrics->mBlockPrefSize);
    SizeNeedsRecalc(metrics->mBlockMinSize);
    CoordNeedsRecalc(metrics->mFlex);
    CoordNeedsRecalc(metrics->mAscent);
  }
}

/* virtual */ nscoord
nsFrame::GetMinWidth(nsRenderingContext *aRenderingContext)
{
  nscoord result = 0;
  DISPLAY_MIN_WIDTH(this, result);
  return result;
}

/* virtual */ nscoord
nsFrame::GetPrefWidth(nsRenderingContext *aRenderingContext)
{
  nscoord result = 0;
  DISPLAY_PREF_WIDTH(this, result);
  return result;
}

/* virtual */ void
nsFrame::AddInlineMinWidth(nsRenderingContext *aRenderingContext,
                           nsIFrame::InlineMinWidthData *aData)
{
  NS_ASSERTION(GetParent(), "Must have a parent if we get here!");
  PRBool canBreak = !CanContinueTextRun() &&
    GetParent()->GetStyleText()->WhiteSpaceCanWrap();
  
  if (canBreak)
    aData->OptionallyBreak(aRenderingContext);
  aData->trailingWhitespace = 0;
  aData->skipWhitespace = PR_FALSE;
  aData->trailingTextFrame = nsnull;
  aData->currentLine += nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                            this, nsLayoutUtils::MIN_WIDTH);
  aData->atStartOfLine = PR_FALSE;
  if (canBreak)
    aData->OptionallyBreak(aRenderingContext);
}

/* virtual */ void
nsFrame::AddInlinePrefWidth(nsRenderingContext *aRenderingContext,
                            nsIFrame::InlinePrefWidthData *aData)
{
  aData->trailingWhitespace = 0;
  aData->skipWhitespace = PR_FALSE;
  nscoord myPref = nsLayoutUtils::IntrinsicForContainer(aRenderingContext, 
                       this, nsLayoutUtils::PREF_WIDTH);
  aData->currentLine = NSCoordSaturatingAdd(aData->currentLine, myPref);
}

void
nsIFrame::InlineMinWidthData::ForceBreak(nsRenderingContext *aRenderingContext)
{
  currentLine -= trailingWhitespace;
  prevLines = NS_MAX(prevLines, currentLine);
  currentLine = trailingWhitespace = 0;

  for (PRUint32 i = 0, i_end = floats.Length(); i != i_end; ++i) {
    nsIFrame *floatFrame = floats[i];
    nscoord float_min =
      nsLayoutUtils::IntrinsicForContainer(aRenderingContext, floatFrame,
                                           nsLayoutUtils::MIN_WIDTH);
    if (float_min > prevLines)
      prevLines = float_min;
  }
  floats.Clear();
  trailingTextFrame = nsnull;
  skipWhitespace = PR_TRUE;
}

void
nsIFrame::InlineMinWidthData::OptionallyBreak(nsRenderingContext *aRenderingContext,
                                              nscoord aHyphenWidth)
{
  trailingTextFrame = nsnull;

  // If we can fit more content into a smaller width by staying on this
  // line (because we're still at a negative offset due to negative
  // text-indent or negative margin), don't break.  Otherwise, do the
  // same as ForceBreak.  it doesn't really matter when we accumulate
  // floats.
  if (currentLine + aHyphenWidth < 0 || atStartOfLine)
    return;
  currentLine += aHyphenWidth;
  ForceBreak(aRenderingContext);
}

void
nsIFrame::InlinePrefWidthData::ForceBreak(nsRenderingContext *aRenderingContext)
{
  if (floats.Length() != 0) {
            // preferred widths accumulated for floats that have already
            // been cleared past
    nscoord floats_done = 0,
            // preferred widths accumulated for floats that have not yet
            // been cleared past
            floats_cur_left = 0,
            floats_cur_right = 0;

    for (PRUint32 i = 0, i_end = floats.Length(); i != i_end; ++i) {
      nsIFrame *floatFrame = floats[i];
      const nsStyleDisplay *floatDisp = floatFrame->GetStyleDisplay();
      if (floatDisp->mBreakType == NS_STYLE_CLEAR_LEFT ||
          floatDisp->mBreakType == NS_STYLE_CLEAR_RIGHT ||
          floatDisp->mBreakType == NS_STYLE_CLEAR_LEFT_AND_RIGHT) {
        nscoord floats_cur = NSCoordSaturatingAdd(floats_cur_left,
                                                  floats_cur_right);
        if (floats_cur > floats_done)
          floats_done = floats_cur;
        if (floatDisp->mBreakType != NS_STYLE_CLEAR_RIGHT)
          floats_cur_left = 0;
        if (floatDisp->mBreakType != NS_STYLE_CLEAR_LEFT)
          floats_cur_right = 0;
      }

      nscoord &floats_cur = floatDisp->mFloats == NS_STYLE_FLOAT_LEFT
                              ? floats_cur_left : floats_cur_right;
      nscoord floatWidth =
          nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                                               floatFrame,
                                               nsLayoutUtils::PREF_WIDTH);
      // Negative-width floats don't change the available space so they
      // shouldn't change our intrinsic line width either.
      floats_cur =
        NSCoordSaturatingAdd(floats_cur, NS_MAX(0, floatWidth));
    }

    nscoord floats_cur =
      NSCoordSaturatingAdd(floats_cur_left, floats_cur_right);
    if (floats_cur > floats_done)
      floats_done = floats_cur;

    currentLine = NSCoordSaturatingAdd(currentLine, floats_done);

    floats.Clear();
  }

  currentLine =
    NSCoordSaturatingSubtract(currentLine, trailingWhitespace, nscoord_MAX);
  prevLines = NS_MAX(prevLines, currentLine);
  currentLine = trailingWhitespace = 0;
  skipWhitespace = PR_TRUE;
}

static void
AddCoord(const nsStyleCoord& aStyle,
         nsRenderingContext* aRenderingContext,
         nsIFrame* aFrame,
         nscoord* aCoord, float* aPercent,
         PRBool aClampNegativeToZero)
{
  switch (aStyle.GetUnit()) {
    case eStyleUnit_Coord: {
      NS_ASSERTION(!aClampNegativeToZero || aStyle.GetCoordValue() >= 0,
                   "unexpected negative value");
      *aCoord += aStyle.GetCoordValue();
      return;
    }
    case eStyleUnit_Percent: {
      NS_ASSERTION(!aClampNegativeToZero || aStyle.GetPercentValue() >= 0.0f,
                   "unexpected negative value");
      *aPercent += aStyle.GetPercentValue();
      return;
    }
    case eStyleUnit_Calc: {
      const nsStyleCoord::Calc *calc = aStyle.GetCalcValue();
      if (aClampNegativeToZero) {
        // This is far from ideal when one is negative and one is positive.
        *aCoord += NS_MAX(calc->mLength, 0);
        *aPercent += NS_MAX(calc->mPercent, 0.0f);
      } else {
        *aCoord += calc->mLength;
        *aPercent += calc->mPercent;
      }
      return;
    }
    default: {
      return;
    }
  }
}

/* virtual */ nsIFrame::IntrinsicWidthOffsetData
nsFrame::IntrinsicWidthOffsets(nsRenderingContext* aRenderingContext)
{
  IntrinsicWidthOffsetData result;

  const nsStyleMargin *styleMargin = GetStyleMargin();
  AddCoord(styleMargin->mMargin.GetLeft(), aRenderingContext, this,
           &result.hMargin, &result.hPctMargin, PR_FALSE);
  AddCoord(styleMargin->mMargin.GetRight(), aRenderingContext, this,
           &result.hMargin, &result.hPctMargin, PR_FALSE);

  const nsStylePadding *stylePadding = GetStylePadding();
  AddCoord(stylePadding->mPadding.GetLeft(), aRenderingContext, this,
           &result.hPadding, &result.hPctPadding, PR_TRUE);
  AddCoord(stylePadding->mPadding.GetRight(), aRenderingContext, this,
           &result.hPadding, &result.hPctPadding, PR_TRUE);

  const nsStyleBorder *styleBorder = GetStyleBorder();
  result.hBorder += styleBorder->GetActualBorderWidth(NS_SIDE_LEFT);
  result.hBorder += styleBorder->GetActualBorderWidth(NS_SIDE_RIGHT);

  const nsStyleDisplay *disp = GetStyleDisplay();
  if (IsThemed(disp)) {
    nsPresContext *presContext = PresContext();

    nsIntMargin border;
    presContext->GetTheme()->GetWidgetBorder(presContext->DeviceContext(),
                                             this, disp->mAppearance,
                                             &border);
    result.hBorder = presContext->DevPixelsToAppUnits(border.LeftRight());

    nsIntMargin padding;
    if (presContext->GetTheme()->GetWidgetPadding(presContext->DeviceContext(),
                                                  this, disp->mAppearance,
                                                  &padding)) {
      result.hPadding = presContext->DevPixelsToAppUnits(padding.LeftRight());
      result.hPctPadding = 0;
    }
  }

  return result;
}

/* virtual */ nsIFrame::IntrinsicSize
nsFrame::GetIntrinsicSize()
{
  return IntrinsicSize(); // default is width/height set to eStyleUnit_None
}

/* virtual */ nsSize
nsFrame::GetIntrinsicRatio()
{
  return nsSize(0, 0);
}

/* virtual */ nsSize
nsFrame::ComputeSize(nsRenderingContext *aRenderingContext,
                     nsSize aCBSize, nscoord aAvailableWidth,
                     nsSize aMargin, nsSize aBorder, nsSize aPadding,
                     PRBool aShrinkWrap)
{
  nsSize result = ComputeAutoSize(aRenderingContext, aCBSize, aAvailableWidth,
                                  aMargin, aBorder, aPadding, aShrinkWrap);
  nsSize boxSizingAdjust(0,0);
  const nsStylePosition *stylePos = GetStylePosition();

  switch (stylePos->mBoxSizing) {
    case NS_STYLE_BOX_SIZING_BORDER:
      boxSizingAdjust += aBorder;
      // fall through
    case NS_STYLE_BOX_SIZING_PADDING:
      boxSizingAdjust += aPadding;
  }
  nscoord boxSizingToMarginEdgeWidth =
    aMargin.width + aBorder.width + aPadding.width - boxSizingAdjust.width;

  // Compute width

  if (stylePos->mWidth.GetUnit() != eStyleUnit_Auto) {
    result.width =
      nsLayoutUtils::ComputeWidthValue(aRenderingContext, this,
        aCBSize.width, boxSizingAdjust.width, boxSizingToMarginEdgeWidth,
        stylePos->mWidth);
  }

  if (stylePos->mMaxWidth.GetUnit() != eStyleUnit_None) {
    nscoord maxWidth =
      nsLayoutUtils::ComputeWidthValue(aRenderingContext, this,
        aCBSize.width, boxSizingAdjust.width, boxSizingToMarginEdgeWidth,
        stylePos->mMaxWidth);
    if (maxWidth < result.width)
      result.width = maxWidth;
  }

  nscoord minWidth =
    nsLayoutUtils::ComputeWidthValue(aRenderingContext, this,
      aCBSize.width, boxSizingAdjust.width, boxSizingToMarginEdgeWidth,
      stylePos->mMinWidth);
  if (minWidth > result.width)
    result.width = minWidth;

  // Compute height

  if (!nsLayoutUtils::IsAutoHeight(stylePos->mHeight, aCBSize.height)) {
    result.height =
      nsLayoutUtils::ComputeHeightValue(aCBSize.height, stylePos->mHeight) -
      boxSizingAdjust.height;
  }

  if (result.height != NS_UNCONSTRAINEDSIZE) {
    if (!nsLayoutUtils::IsAutoHeight(stylePos->mMaxHeight, aCBSize.height)) {
      nscoord maxHeight =
        nsLayoutUtils::ComputeHeightValue(aCBSize.height, stylePos->mMaxHeight) -
        boxSizingAdjust.height;
      if (maxHeight < result.height)
        result.height = maxHeight;
    }

    if (!nsLayoutUtils::IsAutoHeight(stylePos->mMinHeight, aCBSize.height)) {
      nscoord minHeight =
        nsLayoutUtils::ComputeHeightValue(aCBSize.height, stylePos->mMinHeight) -
        boxSizingAdjust.height;
      if (minHeight > result.height)
        result.height = minHeight;
    }
  }

  const nsStyleDisplay *disp = GetStyleDisplay();
  if (IsThemed(disp)) {
    nsIntSize widget(0, 0);
    PRBool canOverride = PR_TRUE;
    nsPresContext *presContext = PresContext();
    presContext->GetTheme()->
      GetMinimumWidgetSize(aRenderingContext, this, disp->mAppearance,
                           &widget, &canOverride);

    nsSize size;
    size.width = presContext->DevPixelsToAppUnits(widget.width);
    size.height = presContext->DevPixelsToAppUnits(widget.height);

    // GMWS() returns border-box; we need content-box
    size.width -= aBorder.width + aPadding.width;
    size.height -= aBorder.height + aPadding.height;

    if (size.height > result.height || !canOverride)
      result.height = size.height;
    if (size.width > result.width || !canOverride)
      result.width = size.width;
  }

  if (result.width < 0)
    result.width = 0;

  if (result.height < 0)
    result.height = 0;

  return result;
}

nsRect
nsIFrame::ComputeTightBounds(gfxContext* aContext) const
{
  return GetVisualOverflowRect();
}

nsRect
nsFrame::ComputeSimpleTightBounds(gfxContext* aContext) const
{
  if (GetStyleOutline()->GetOutlineStyle() != NS_STYLE_BORDER_STYLE_NONE ||
      HasBorder() || !GetStyleBackground()->IsTransparent() ||
      GetStyleDisplay()->mAppearance) {
    // Not necessarily tight, due to clipping, negative
    // outline-offset, and lots of other issues, but that's OK
    return GetVisualOverflowRect();
  }

  nsRect r(0, 0, 0, 0);
  ChildListIterator lists(this);
  for (; !lists.IsDone(); lists.Next()) {
    nsFrameList::Enumerator childFrames(lists.CurrentList());
    for (; !childFrames.AtEnd(); childFrames.Next()) {
      nsIFrame* child = childFrames.get();
      r.UnionRect(r, child->ComputeTightBounds(aContext) + child->GetPosition());
    }
  }
  return r;
}

/* virtual */ nsSize
nsFrame::ComputeAutoSize(nsRenderingContext *aRenderingContext,
                         nsSize aCBSize, nscoord aAvailableWidth,
                         nsSize aMargin, nsSize aBorder, nsSize aPadding,
                         PRBool aShrinkWrap)
{
  // Use basic shrink-wrapping as a default implementation.
  nsSize result(0xdeadbeef, NS_UNCONSTRAINEDSIZE);

  // don't bother setting it if the result won't be used
  if (GetStylePosition()->mWidth.GetUnit() == eStyleUnit_Auto) {
    nscoord availBased = aAvailableWidth - aMargin.width - aBorder.width -
                         aPadding.width;
    result.width = ShrinkWidthToFit(aRenderingContext, availBased);
  }
  return result;
}

nscoord
nsFrame::ShrinkWidthToFit(nsRenderingContext *aRenderingContext,
                          nscoord aWidthInCB)
{
  nscoord result;
  nscoord minWidth = GetMinWidth(aRenderingContext);
  if (minWidth > aWidthInCB) {
    result = minWidth;
  } else {
    nscoord prefWidth = GetPrefWidth(aRenderingContext);
    if (prefWidth > aWidthInCB) {
      result = aWidthInCB;
    } else {
      result = prefWidth;
    }
  }
  return result;
}

NS_IMETHODIMP
nsFrame::WillReflow(nsPresContext* aPresContext)
{
#ifdef DEBUG_dbaron_off
  // bug 81268
  NS_ASSERTION(!(mState & NS_FRAME_IN_REFLOW),
               "nsFrame::WillReflow: frame is already in reflow");
#endif

  NS_FRAME_TRACE_MSG(NS_FRAME_TRACE_CALLS,
                     ("WillReflow: oldState=%x", mState));
  mState |= NS_FRAME_IN_REFLOW;
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::DidReflow(nsPresContext*           aPresContext,
                   const nsHTMLReflowState*  aReflowState,
                   nsDidReflowStatus         aStatus)
{
  NS_FRAME_TRACE_MSG(NS_FRAME_TRACE_CALLS,
                     ("nsFrame::DidReflow: aStatus=%d", aStatus));
  if (NS_FRAME_REFLOW_FINISHED == aStatus) {
    mState &= ~(NS_FRAME_IN_REFLOW | NS_FRAME_FIRST_REFLOW | NS_FRAME_IS_DIRTY |
                NS_FRAME_HAS_DIRTY_CHILDREN);
  }

  // Notify the percent height observer if there is a percent height.
  // The observer may be able to initiate another reflow with a computed
  // height. This happens in the case where a table cell has no computed
  // height but can fabricate one when the cell height is known.
  if (aReflowState && aReflowState->mPercentHeightObserver &&
      !GetPrevInFlow()) {
    const nsStyleCoord &height = aReflowState->mStylePosition->mHeight;
    if (height.HasPercent()) {
      aReflowState->mPercentHeightObserver->NotifyPercentHeight(*aReflowState);
    }
  }

  return NS_OK;
}

/* virtual */ PRBool
nsFrame::CanContinueTextRun() const
{
  // By default, a frame will *not* allow a text run to be continued
  // through it.
  return PR_FALSE;
}

NS_IMETHODIMP
nsFrame::Reflow(nsPresContext*          aPresContext,
                nsHTMLReflowMetrics&     aDesiredSize,
                const nsHTMLReflowState& aReflowState,
                nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsFrame");
  aDesiredSize.width = 0;
  aDesiredSize.height = 0;
  aStatus = NS_FRAME_COMPLETE;
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::CharacterDataChanged(CharacterDataChangeInfo* aInfo)
{
  NS_NOTREACHED("should only be called for text frames");
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::AttributeChanged(PRInt32         aNameSpaceID,
                          nsIAtom*        aAttribute,
                          PRInt32         aModType)
{
  return NS_OK;
}

// Flow member functions

nsSplittableType
nsFrame::GetSplittableType() const
{
  return NS_FRAME_NOT_SPLITTABLE;
}

nsIFrame* nsFrame::GetPrevContinuation() const
{
  return nsnull;
}

NS_IMETHODIMP nsFrame::SetPrevContinuation(nsIFrame* aPrevContinuation)
{
  // Ignore harmless requests to set it to NULL
  if (aPrevContinuation) {
    NS_ERROR("not splittable");
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  
  return NS_OK;
}

nsIFrame* nsFrame::GetNextContinuation() const
{
  return nsnull;
}

NS_IMETHODIMP nsFrame::SetNextContinuation(nsIFrame*)
{
  NS_ERROR("not splittable");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsIFrame* nsFrame::GetPrevInFlowVirtual() const
{
  return nsnull;
}

NS_IMETHODIMP nsFrame::SetPrevInFlow(nsIFrame* aPrevInFlow)
{
  // Ignore harmless requests to set it to NULL
  if (aPrevInFlow) {
    NS_ERROR("not splittable");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  return NS_OK;
}

nsIFrame* nsFrame::GetNextInFlowVirtual() const
{
  return nsnull;
}

NS_IMETHODIMP nsFrame::SetNextInFlow(nsIFrame*)
{
  NS_ERROR("not splittable");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsIFrame* nsIFrame::GetTailContinuation()
{
  nsIFrame* frame = this;
  while (frame->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER) {
    frame = frame->GetPrevContinuation();
    NS_ASSERTION(frame, "first continuation can't be overflow container");
  }
  for (nsIFrame* next = frame->GetNextContinuation();
       next && !(next->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER);
       next = frame->GetNextContinuation())  {
    frame = next;
  }
  NS_POSTCONDITION(frame, "illegal state in continuation chain.");
  return frame;
}

NS_DECLARE_FRAME_PROPERTY(ViewProperty, nsnull)

// Associated view object
nsIView*
nsIFrame::GetView() const
{
  // Check the frame state bit and see if the frame has a view
  if (!(GetStateBits() & NS_FRAME_HAS_VIEW))
    return nsnull;

  // Check for a property on the frame
  void* value = Properties().Get(ViewProperty());
  NS_ASSERTION(value, "frame state bit was set but frame has no view");
  return static_cast<nsIView*>(value);
}

/* virtual */ nsIView*
nsIFrame::GetViewExternal() const
{
  return GetView();
}

nsresult
nsIFrame::SetView(nsIView* aView)
{
  if (aView) {
    aView->SetClientData(this);

    // Set a property on the frame
    Properties().Set(ViewProperty(), aView);

    // Set the frame state bit that says the frame has a view
    AddStateBits(NS_FRAME_HAS_VIEW);

    // Let all of the ancestors know they have a descendant with a view.
    for (nsIFrame* f = GetParent();
         f && !(f->GetStateBits() & NS_FRAME_HAS_CHILD_WITH_VIEW);
         f = f->GetParent())
      f->AddStateBits(NS_FRAME_HAS_CHILD_WITH_VIEW);
  }

  return NS_OK;
}

nsIFrame* nsIFrame::GetAncestorWithViewExternal() const
{
  return GetAncestorWithView();
}

// Find the first geometric parent that has a view
nsIFrame* nsIFrame::GetAncestorWithView() const
{
  for (nsIFrame* f = mParent; nsnull != f; f = f->GetParent()) {
    if (f->HasView()) {
      return f;
    }
  }
  return nsnull;
}

// virtual
nsPoint nsIFrame::GetOffsetToExternal(const nsIFrame* aOther) const
{
  return GetOffsetTo(aOther);
}

nsPoint nsIFrame::GetOffsetTo(const nsIFrame* aOther) const
{
  NS_PRECONDITION(aOther,
                  "Must have frame for destination coordinate system!");

  NS_ASSERTION(PresContext() == aOther->PresContext(),
               "GetOffsetTo called on frames in different documents");

  nsPoint offset(0, 0);
  const nsIFrame* f;
  for (f = this; f != aOther && f; f = f->GetParent()) {
    offset += f->GetPosition();
  }

  if (f != aOther) {
    // Looks like aOther wasn't an ancestor of |this|.  So now we have
    // the root-frame-relative position of |this| in |offset|.  Convert back
    // to the coordinates of aOther
    while (aOther) {
      offset -= aOther->GetPosition();
      aOther = aOther->GetParent();
    }
  }

  return offset;
}

nsPoint nsIFrame::GetOffsetToCrossDoc(const nsIFrame* aOther) const
{
  return GetOffsetToCrossDoc(aOther, PresContext()->AppUnitsPerDevPixel());
}

nsPoint
nsIFrame::GetOffsetToCrossDoc(const nsIFrame* aOther, const PRInt32 aAPD) const
{
  NS_PRECONDITION(aOther,
                  "Must have frame for destination coordinate system!");
  NS_ASSERTION(PresContext()->GetRootPresContext() ==
                 aOther->PresContext()->GetRootPresContext(),
               "trying to get the offset between frames in different document "
               "hierarchies?");
  if (PresContext()->GetRootPresContext() !=
        aOther->PresContext()->GetRootPresContext()) {
    // crash right away, we are almost certainly going to crash anyway.
    *(static_cast<PRInt32*>(nsnull)) = 3;
  }

  const nsIFrame* root = nsnull;
  // offset will hold the final offset
  // docOffset holds the currently accumulated offset at the current APD, it
  // will be converted and added to offset when the current APD changes.
  nsPoint offset(0, 0), docOffset(0, 0);
  const nsIFrame* f = this;
  PRInt32 currAPD = PresContext()->AppUnitsPerDevPixel();
  while (f && f != aOther) {
    docOffset += f->GetPosition();
    nsIFrame* parent = f->GetParent();
    if (parent) {
      f = parent;
    } else {
      nsPoint newOffset(0, 0);
      root = f;
      f = nsLayoutUtils::GetCrossDocParentFrame(f, &newOffset);
      PRInt32 newAPD = f ? f->PresContext()->AppUnitsPerDevPixel() : 0;
      if (!f || newAPD != currAPD) {
        // Convert docOffset to the right APD and add it to offset.
        offset += docOffset.ConvertAppUnits(currAPD, aAPD);
        docOffset.x = docOffset.y = 0;
      }
      currAPD = newAPD;
      docOffset += newOffset;
    }
  }
  if (f == aOther) {
    offset += docOffset.ConvertAppUnits(currAPD, aAPD);
  } else {
    // Looks like aOther wasn't an ancestor of |this|.  So now we have
    // the root-document-relative position of |this| in |offset|. Subtract the
    // root-document-relative position of |aOther| from |offset|.
    // This call won't try to recurse again because root is an ancestor of
    // aOther.
    nsPoint negOffset = aOther->GetOffsetToCrossDoc(root, aAPD);
    offset -= negOffset;
  }

  return offset;
}

// virtual
nsIntRect nsIFrame::GetScreenRectExternal() const
{
  return GetScreenRect();
}

nsIntRect nsIFrame::GetScreenRect() const
{
  return GetScreenRectInAppUnits().ToNearestPixels(PresContext()->AppUnitsPerCSSPixel());
}

// virtual
nsRect nsIFrame::GetScreenRectInAppUnitsExternal() const
{
  return GetScreenRectInAppUnits();
}

nsRect nsIFrame::GetScreenRectInAppUnits() const
{
  nsPresContext* presContext = PresContext();
  nsIFrame* rootFrame =
    presContext->PresShell()->FrameManager()->GetRootFrame();
  nsPoint rootScreenPos(0, 0);
  nsPoint rootFrameOffsetInParent(0, 0);
  nsIFrame* rootFrameParent =
    nsLayoutUtils::GetCrossDocParentFrame(rootFrame, &rootFrameOffsetInParent);
  if (rootFrameParent) {
    nsRect parentScreenRectAppUnits = rootFrameParent->GetScreenRectInAppUnits();
    nsPresContext* parentPresContext = rootFrameParent->PresContext();
    double parentScale = double(presContext->AppUnitsPerDevPixel())/
        parentPresContext->AppUnitsPerDevPixel();
    nsPoint rootPt = parentScreenRectAppUnits.TopLeft() + rootFrameOffsetInParent;
    rootScreenPos.x = NS_round(parentScale*rootPt.x);
    rootScreenPos.y = NS_round(parentScale*rootPt.y);
  } else {
    nsCOMPtr<nsIWidget> rootWidget;
    presContext->PresShell()->GetViewManager()->GetRootWidget(getter_AddRefs(rootWidget));
    if (rootWidget) {
      nsIntPoint rootDevPx = rootWidget->WidgetToScreenOffset();
      rootScreenPos.x = presContext->DevPixelsToAppUnits(rootDevPx.x);
      rootScreenPos.y = presContext->DevPixelsToAppUnits(rootDevPx.y);
    }
  }

  return nsRect(rootScreenPos + GetOffsetTo(rootFrame), GetSize());
}

// Returns the offset from this frame to the closest geometric parent that
// has a view. Also returns the containing view or null in case of error
NS_IMETHODIMP nsFrame::GetOffsetFromView(nsPoint&  aOffset,
                                         nsIView** aView) const
{
  NS_PRECONDITION(nsnull != aView, "null OUT parameter pointer");
  nsIFrame* frame = (nsIFrame*)this;

  *aView = nsnull;
  aOffset.MoveTo(0, 0);
  do {
    aOffset += frame->GetPosition();
    frame = frame->GetParent();
  } while (frame && !frame->HasView());
  if (frame)
    *aView = frame->GetView();
  return NS_OK;
}

/* virtual */ PRBool
nsIFrame::AreAncestorViewsVisible() const
{
  const nsIFrame* parent;
  for (const nsIFrame* f = this; f; f = parent) {
    nsIView* view = f->GetView();
    if (view && view->GetVisibility() == nsViewVisibility_kHide) {
      return PR_FALSE;
    }
    parent = f->GetParent();
    if (!parent) {
      parent = nsLayoutUtils::GetCrossDocParentFrame(f);
      if (parent && parent->PresContext()->IsChrome() &&
          !f->PresContext()->IsChrome()) {
        // Don't look beyond chrome/content boundary ... if the chrome
        // has hidden a content docshell, the content in the content
        // docshell shouldn't be affected (e.g. it should remain focusable).
        break;
      }
    }
  }
  return PR_TRUE;
}

nsIWidget*
nsIFrame::GetNearestWidget() const
{
  return GetClosestView()->GetNearestWidget(nsnull);
}

nsIWidget*
nsIFrame::GetNearestWidget(nsPoint& aOffset) const
{
  nsPoint offsetToView;
  nsPoint offsetToWidget;
  nsIWidget* widget =
    GetClosestView(&offsetToView)->GetNearestWidget(&offsetToWidget);
  aOffset = offsetToView + offsetToWidget;
  return widget;
}

nsIAtom*
nsFrame::GetType() const
{
  return nsnull;
}

PRBool
nsIFrame::IsLeaf() const
{
  return PR_TRUE;
}

Layer*
nsIFrame::InvalidateLayer(const nsRect& aDamageRect, PRUint32 aDisplayItemKey)
{
  NS_ASSERTION(aDisplayItemKey > 0, "Need a key");

  Layer* layer = FrameLayerBuilder::GetDedicatedLayer(this, aDisplayItemKey);
  if (!layer) {
    Invalidate(aDamageRect);
    return nsnull;
  }

  PRUint32 flags = INVALIDATE_NO_THEBES_LAYERS;
  if (aDisplayItemKey == nsDisplayItem::TYPE_VIDEO ||
      aDisplayItemKey == nsDisplayItem::TYPE_PLUGIN ||
      aDisplayItemKey == nsDisplayItem::TYPE_CANVAS) {
    flags |= INVALIDATE_NO_UPDATE_LAYER_TREE;
  }

  InvalidateWithFlags(aDamageRect, flags);
  return layer;
}

void
nsIFrame::InvalidateTransformLayer()
{
  NS_ASSERTION(mParent, "How can a viewport frame have a transform?");

  PRBool hasLayer =
      FrameLayerBuilder::GetDedicatedLayer(this, nsDisplayItem::TYPE_TRANSFORM) != nsnull;
  // Invalidate post-transform area in the parent. We have to invalidate
  // in the parent because our transform style may have changed from what was
  // used to paint this frame.
  // It's OK to bypass the SVG effects processing and other processing
  // performed if we called this->InvalidateWithFlags, because those effects
  // are performed before applying transforms.
  mParent->InvalidateInternal(GetVisualOverflowRect() + GetPosition(),
                              0, 0, this,
                              hasLayer ? INVALIDATE_NO_THEBES_LAYERS : 0);
}

class LayerActivity {
public:
  LayerActivity(nsIFrame* aFrame) : mFrame(aFrame), mChangeHint(nsChangeHint(0)) {}
  ~LayerActivity();
  nsExpirationState* GetExpirationState() { return &mState; }

  nsIFrame* mFrame;
  nsExpirationState mState;
  // mChangeHint can be some combination of nsChangeHint_UpdateOpacityLayer and
  // nsChangeHint_UpdateTransformLayer (or neither)
  // The presence of those bits indicates whether opacity or transform
  // changes have been detected.
  nsChangeHint mChangeHint;
};

class LayerActivityTracker : public nsExpirationTracker<LayerActivity,4> {
public:
  // 75-100ms is a good timeout period. We use 4 generations of 25ms each.
  enum { GENERATION_MS = 100 };
  LayerActivityTracker()
    : nsExpirationTracker<LayerActivity,4>(GENERATION_MS) {}
  ~LayerActivityTracker() {
    AgeAllGenerations();
  }

  virtual void NotifyExpired(LayerActivity* aObject);
};

static LayerActivityTracker* gLayerActivityTracker = nsnull;

LayerActivity::~LayerActivity()
{
  if (mFrame) {
    NS_ASSERTION(gLayerActivityTracker, "Should still have a tracker");
    gLayerActivityTracker->RemoveObject(this);
  }
}

static void DestroyLayerActivity(void* aPropertyValue)
{
  delete static_cast<LayerActivity*>(aPropertyValue);
}

NS_DECLARE_FRAME_PROPERTY(LayerActivityProperty, DestroyLayerActivity)

void
LayerActivityTracker::NotifyExpired(LayerActivity* aObject)
{
  RemoveObject(aObject);

  nsIFrame* f = aObject->mFrame;
  aObject->mFrame = nsnull;
  f->Properties().Delete(LayerActivityProperty());
  f->InvalidateFrameSubtree();
}

void
nsIFrame::MarkLayersActive(nsChangeHint aChangeHint)
{
  FrameProperties properties = Properties();
  LayerActivity* layerActivity =
    static_cast<LayerActivity*>(properties.Get(LayerActivityProperty()));
  if (layerActivity) {
    gLayerActivityTracker->MarkUsed(layerActivity);
  } else {
    if (!gLayerActivityTracker) {
      gLayerActivityTracker = new LayerActivityTracker();
    }
    layerActivity = new LayerActivity(this);
    gLayerActivityTracker->AddObject(layerActivity);
    properties.Set(LayerActivityProperty(), layerActivity);
  }
  NS_UpdateHint(layerActivity->mChangeHint, aChangeHint);
}

PRBool
nsIFrame::AreLayersMarkedActive()
{
  return Properties().Get(LayerActivityProperty()) != nsnull;
}

PRBool
nsIFrame::AreLayersMarkedActive(nsChangeHint aChangeHint)
{
  LayerActivity* layerActivity =
    static_cast<LayerActivity*>(Properties().Get(LayerActivityProperty()));
  return layerActivity && (layerActivity->mChangeHint & aChangeHint);
}

/* static */ void
nsFrame::ShutdownLayerActivityTimer()
{
  delete gLayerActivityTracker;
  gLayerActivityTracker = nsnull;
}

void
nsIFrame::InvalidateWithFlags(const nsRect& aDamageRect, PRUint32 aFlags)
{
  if (aDamageRect.IsEmpty()) {
    return;
  }

  // Don't allow invalidates to do anything when
  // painting is suppressed.
  nsIPresShell *shell = PresContext()->GetPresShell();
  if (shell) {
    if (shell->IsPaintingSuppressed())
      return;
  }

  InvalidateInternal(aDamageRect, 0, 0, nsnull, aFlags);
}

/**
 * Helper function that funnels an InvalidateInternal request up to the
 * parent.  This function is used so that if MOZ_SVG is not defined, we still
 * have unified control paths in the InvalidateInternal chain.
 *
 * @param aDamageRect The rect to invalidate.
 * @param aX The x offset from the origin of this frame to the rectangle.
 * @param aY The y offset from the origin of this frame to the rectangle.
 * @param aImmediate Whether to redraw immediately.
 * @return None, though this funnels the request up to the parent frame.
 */
void
nsIFrame::InvalidateInternalAfterResize(const nsRect& aDamageRect, nscoord aX,
                                        nscoord aY, PRUint32 aFlags)
{
  /* If we're a transformed frame, then we need to apply our transform to the
   * damage rectangle so that the redraw correctly redraws the transformed
   * region.  We're moved over aX and aY from our origin, but since this aX
   * and aY is contained within our border, we need to scoot back by -aX and
   * -aY to get back to the origin of the transform.
   *
   * There's one more problem, though, and that's that we don't know what
   * coordinate space this rectangle is in.  Sometimes it's in the local
   * coordinate space for the frame, and sometimes its in the transformed
   * coordinate space.  If we get it wrong, we'll display incorrectly.  Until I
   * find a better fix for this problem, we'll invalidate the union of the two
   * rectangles (original rectangle and transformed rectangle).  At least one of
   * these will be correct.
   *
   * When we are preserving-3d, we can have arbitrary hierarchies of preserved 3d
   * children. The computed transform on these children is relative to the root
   * transform object in the hierarchy, not necessarily their direct ancestor.
   * In this case we transform by the child's transform, and mark the rectangle
   * as being transformed until it is passed up to the root of the hierarchy.
   *
   * See bug #452496 for more details.
   */

  // Check the transformed flags and remove it
  PRBool rectIsTransformed = (aFlags & INVALIDATE_ALREADY_TRANSFORMED);
  if (!Preserves3D()) {
    // We only want to remove the flag if we aren't preserving 3d. Otherwise
    // the rect will already have been transformed into the root preserve-3d
    // frame coordinate space, and we should continue passing it up without
    // further transforms.
    aFlags &= ~INVALIDATE_ALREADY_TRANSFORMED;
  }

  if ((mState & NS_FRAME_HAS_CONTAINER_LAYER) &&
      !(aFlags & INVALIDATE_NO_THEBES_LAYERS)) {
    // XXX for now I'm going to assume this is in the local coordinate space
    // This only matters for frames with transforms and retained layers,
    // which can't happen right now since transforms trigger fallback
    // rendering and the display items that trigger layers are nested inside
    // the nsDisplayTransform
    // XXX need to set INVALIDATE_NO_THEBES_LAYERS for certain kinds of
    // invalidation, e.g. video update, 'opacity' change
    FrameLayerBuilder::InvalidateThebesLayerContents(this,
        aDamageRect + nsPoint(aX, aY));
    // Don't need to invalidate any more Thebes layers
    aFlags |= INVALIDATE_NO_THEBES_LAYERS;
    if (aFlags & INVALIDATE_ONLY_THEBES_LAYERS) {
      return;
    }
  }
  if (IsTransformed() && !rectIsTransformed) {
    nsRect newDamageRect;
    newDamageRect.UnionRect(nsDisplayTransform::TransformRectOut
                            (aDamageRect, this, nsPoint(-aX, -aY)), aDamageRect);

    // If we are preserving 3d, then our computed transform includes that of any
    // ancestor frames that also preserve 3d. Mark the rectangle as already being
    // transformed into the parent's coordinate space.
    if (Preserves3D()) {
      aFlags |= INVALIDATE_ALREADY_TRANSFORMED;
    }

    GetParent()->
      InvalidateInternal(newDamageRect, aX + mRect.x, aY + mRect.y, this,
                         aFlags);
  }
  else 
    GetParent()->
      InvalidateInternal(aDamageRect, aX + mRect.x, aY + mRect.y, this, aFlags);
}

void
nsIFrame::InvalidateInternal(const nsRect& aDamageRect, nscoord aX, nscoord aY,
                             nsIFrame* aForChild, PRUint32 aFlags)
{
  nsSVGEffects::InvalidateDirectRenderingObservers(this);
  if (nsSVGIntegrationUtils::UsingEffectsForFrame(this)) {
    nsRect r = nsSVGIntegrationUtils::GetInvalidAreaForChangedSource(this,
            aDamageRect + nsPoint(aX, aY));
    /* Rectangle is now in our own local space, so aX and aY are effectively
     * zero.  Thus we'll pretend that the entire time this was in our own
     * local coordinate space and do any remaining processing.
     */
    InvalidateInternalAfterResize(r, 0, 0, aFlags);
    return;
  }
  
  InvalidateInternalAfterResize(aDamageRect, aX, aY, aFlags);
}

gfx3DMatrix
nsIFrame::GetTransformMatrix(nsIFrame **aOutAncestor)
{
  NS_PRECONDITION(aOutAncestor, "Need a place to put the ancestor!");

  /* If we're transformed, we want to hand back the combination
   * transform/translate matrix that will apply our current transform, then
   * shift us to our parent.
   */
  if (IsTransformed()) {
    /* Compute the delta to the parent, which we need because we are converting
     * coordinates to our parent.
     */
    NS_ASSERTION(nsLayoutUtils::GetCrossDocParentFrame(this), "Cannot transform the viewport frame!");
    PRInt32 scaleFactor = PresContext()->AppUnitsPerDevPixel();

    gfx3DMatrix result =
      nsDisplayTransform::GetResultingTransformMatrix(this, nsPoint(0, 0),
                                                      scaleFactor, nsnull, aOutAncestor);
    nsPoint delta = GetOffsetToCrossDoc(*aOutAncestor);
    /* Combine the raw transform with a translation to our parent. */
    result *= gfx3DMatrix::Translation
      (NSAppUnitsToFloatPixels(delta.x, scaleFactor),
       NSAppUnitsToFloatPixels(delta.y, scaleFactor),
       0.0f);
    return result;
  }
  
  *aOutAncestor = nsLayoutUtils::GetCrossDocParentFrame(this);
  
  /* Otherwise, we're not transformed.  In that case, we'll walk up the frame
   * tree until we either hit the root frame or something that may be
   * transformed.  We'll then change coordinates into that frame, since we're
   * guaranteed that nothing in-between can be transformed.  First, however,
   * we have to check to see if we have a parent.  If not, we'll set the
   * outparam to null (indicating that there's nothing left) and will hand back
   * the identity matrix.
   */
  if (!*aOutAncestor)
    return gfx3DMatrix();
  
  /* Keep iterating while the frame can't possibly be transformed. */
  while (!(*aOutAncestor)->IsTransformed()) {
    /* If no parent, stop iterating.  Otherwise, update the ancestor. */
    nsIFrame* parent = nsLayoutUtils::GetCrossDocParentFrame(*aOutAncestor);
    if (!parent)
      break;

    *aOutAncestor = parent;
  }

  NS_ASSERTION(*aOutAncestor, "Somehow ended up with a null ancestor...?");

  /* Translate from this frame to our ancestor, if it exists.  That's the
   * entire transform, so we're done.
   */
  nsPoint delta = GetOffsetToCrossDoc(*aOutAncestor);
  PRInt32 scaleFactor = PresContext()->AppUnitsPerDevPixel();
  return gfx3DMatrix().Translation
    (NSAppUnitsToFloatPixels(delta.x, scaleFactor),
     NSAppUnitsToFloatPixels(delta.y, scaleFactor),
     0.0f);
}

void
nsIFrame::InvalidateRectDifference(const nsRect& aR1, const nsRect& aR2)
{
  nsRect sizeHStrip, sizeVStrip;
  nsLayoutUtils::GetRectDifferenceStrips(aR1, aR2, &sizeHStrip, &sizeVStrip);
  Invalidate(sizeVStrip);
  Invalidate(sizeHStrip);
}

void
nsIFrame::InvalidateFrameSubtree()
{
  Invalidate(GetVisualOverflowRectRelativeToSelf());
  FrameLayerBuilder::InvalidateThebesLayersInSubtree(this);
}

void
nsIFrame::InvalidateOverflowRect()
{
  Invalidate(GetVisualOverflowRectRelativeToSelf());
}

NS_DECLARE_FRAME_PROPERTY(DeferInvalidatesProperty, nsIFrame::DestroyRegion)

void
nsIFrame::InvalidateRoot(const nsRect& aDamageRect, PRUint32 aFlags)
{
  NS_ASSERTION(nsLayoutUtils::GetDisplayRootFrame(this) == this,
               "Can only call this on display roots");

  if ((mState & NS_FRAME_HAS_CONTAINER_LAYER) &&
      !(aFlags & INVALIDATE_NO_THEBES_LAYERS)) {
    FrameLayerBuilder::InvalidateThebesLayerContents(this, aDamageRect);
    if (aFlags & INVALIDATE_ONLY_THEBES_LAYERS) {
      return;
    }
  }

  PRUint32 flags =
    (aFlags & INVALIDATE_IMMEDIATE) ? NS_VMREFRESH_IMMEDIATE : NS_VMREFRESH_NO_SYNC;

  nsRect rect = aDamageRect;
  nsRegion* excludeRegion = static_cast<nsRegion*>
    (Properties().Get(DeferInvalidatesProperty()));
  if (excludeRegion) {
    flags = NS_VMREFRESH_DEFERRED;

    if (aFlags & INVALIDATE_EXCLUDE_CURRENT_PAINT) {
      nsRegion r;
      r.Sub(rect, *excludeRegion);
      if (r.IsEmpty())
        return;
      rect = r.GetBounds();
    }
  }

  if (!(aFlags & INVALIDATE_NO_UPDATE_LAYER_TREE)) {
    AddStateBits(NS_FRAME_UPDATE_LAYER_TREE);
  }

  nsIView* view = GetView();
  NS_ASSERTION(view, "This can only be called on frames with views");
  view->GetViewManager()->UpdateViewNoSuppression(view, rect, flags);
}

void
nsIFrame::BeginDeferringInvalidatesForDisplayRoot(const nsRegion& aExcludeRegion)
{
  NS_ASSERTION(nsLayoutUtils::GetDisplayRootFrame(this) == this,
               "Can only call this on display roots");
  Properties().Set(DeferInvalidatesProperty(), new nsRegion(aExcludeRegion));
}

void
nsIFrame::EndDeferringInvalidatesForDisplayRoot()
{
  NS_ASSERTION(nsLayoutUtils::GetDisplayRootFrame(this) == this,
               "Can only call this on display roots");
  Properties().Delete(DeferInvalidatesProperty());
}

/**
 * @param aAnyOutlineOrEffects set to true if this frame has any
 * outline, SVG effects or box shadows that mean we need to invalidate
 * the whole overflow area if the frame's size changes.
 */
static nsRect
ComputeOutlineAndEffectsRect(nsIFrame* aFrame, PRBool* aAnyOutlineOrEffects,
                             const nsRect& aOverflowRect,
                             const nsSize& aNewSize,
                             PRBool aStoreRectProperties) {
  nsRect r = aOverflowRect;
  *aAnyOutlineOrEffects = PR_FALSE;

  // box-shadow
  nsCSSShadowArray* boxShadows = aFrame->GetStyleBorder()->mBoxShadow;
  if (boxShadows) {
    nsRect shadows;
    PRInt32 A2D = aFrame->PresContext()->AppUnitsPerDevPixel();
    for (PRUint32 i = 0; i < boxShadows->Length(); ++i) {
      nsRect tmpRect(nsPoint(0, 0), aNewSize);
      nsCSSShadowItem* shadow = boxShadows->ShadowAt(i);

      // inset shadows are never painted outside the frame
      if (shadow->mInset)
        continue;

      tmpRect.MoveBy(nsPoint(shadow->mXOffset, shadow->mYOffset));
      tmpRect.Inflate(shadow->mSpread, shadow->mSpread);
      tmpRect.Inflate(
        nsContextBoxBlur::GetBlurRadiusMargin(shadow->mRadius, A2D));

      shadows.UnionRect(shadows, tmpRect);
    }
    r.UnionRect(r, shadows);
    *aAnyOutlineOrEffects = PR_TRUE;
  }

  const nsStyleOutline* outline = aFrame->GetStyleOutline();
  PRUint8 outlineStyle = outline->GetOutlineStyle();
  if (outlineStyle != NS_STYLE_BORDER_STYLE_NONE) {
    nscoord width;
#ifdef DEBUG
    PRBool result = 
#endif
      outline->GetOutlineWidth(width);
    NS_ASSERTION(result, "GetOutlineWidth had no cached outline width");
    if (width > 0) {
      if (aStoreRectProperties) {
        aFrame->Properties().
          Set(nsIFrame::OutlineInnerRectProperty(), new nsRect(r));
      }

      nscoord offset = outline->mOutlineOffset;
      nscoord inflateBy = NS_MAX(width + offset, 0);
      // FIXME (bug 599652): We probably want outline to be drawn around
      // something smaller than the visual overflow rect (perhaps the
      // scrollable overflow rect is correct).  When we change that, we
      // need to keep this code (and the storing of properties just
      // above) in sync with GetOutlineInnerRect in nsCSSRendering.cpp.
      r.Inflate(inflateBy, inflateBy);
      *aAnyOutlineOrEffects = PR_TRUE;
    }
  }
  
  // Note that we don't remove the outlineInnerRect if a frame loses outline
  // style. That would require an extra property lookup for every frame,
  // or a new frame state bit to track whether a property had been stored,
  // or something like that. It's not worth doing that here. At most it's
  // only one heap-allocated rect per frame and it will be cleaned up when
  // the frame dies.

  if (nsSVGIntegrationUtils::UsingEffectsForFrame(aFrame)) {
    *aAnyOutlineOrEffects = PR_TRUE;
    if (aStoreRectProperties) {
      aFrame->Properties().
        Set(nsIFrame::PreEffectsBBoxProperty(), new nsRect(r));
    }
    r = nsSVGIntegrationUtils::ComputeFrameEffectsRect(aFrame, r);
  }

  return r;
}

nsPoint
nsIFrame::GetRelativeOffset(const nsStyleDisplay* aDisplay) const
{
  if (!aDisplay || NS_STYLE_POSITION_RELATIVE == aDisplay->mPosition) {
    nsPoint *offsets = static_cast<nsPoint*>
      (Properties().Get(ComputedOffsetProperty()));
    if (offsets) {
      return *offsets;
    }
  }
  return nsPoint(0,0);
}

nsRect
nsIFrame::GetOverflowRect(nsOverflowType aType) const
{
  NS_ABORT_IF_FALSE(aType == eVisualOverflow || aType == eScrollableOverflow,
                    "unexpected type");

  // Note that in some cases the overflow area might not have been
  // updated (yet) to reflect any outline set on the frame or the area
  // of child frames. That's OK because any reflow that updates these
  // areas will invalidate the appropriate area, so any (mis)uses of
  // this method will be fixed up.

  if (mOverflow.mType == NS_FRAME_OVERFLOW_LARGE) {
    // there is an overflow rect, and it's not stored as deltas but as
    // a separately-allocated rect
    return static_cast<nsOverflowAreas*>(const_cast<nsIFrame*>(this)->
             GetOverflowAreasProperty())->Overflow(aType);
  }

  if (aType == eVisualOverflow &&
      mOverflow.mType != NS_FRAME_OVERFLOW_NONE) {
    return GetVisualOverflowFromDeltas();
  }

  return nsRect(nsPoint(0, 0), GetSize());
}

nsOverflowAreas
nsIFrame::GetOverflowAreas() const
{
  if (mOverflow.mType == NS_FRAME_OVERFLOW_LARGE) {
    // there is an overflow rect, and it's not stored as deltas but as
    // a separately-allocated rect
    return *const_cast<nsIFrame*>(this)->GetOverflowAreasProperty();
  }

  return nsOverflowAreas(GetVisualOverflowFromDeltas(),
                         nsRect(nsPoint(0, 0), GetSize()));
}

nsRect
nsIFrame::GetScrollableOverflowRectRelativeToParent() const
{
  return GetScrollableOverflowRect() + mRect.TopLeft();
}

nsRect
nsIFrame::GetVisualOverflowRectRelativeToSelf() const
{
  if (IsTransformed()) {
    nsRect* preTransformBBox = static_cast<nsRect*>
      (Properties().Get(PreTransformBBoxProperty()));
    if (preTransformBBox)
      return *preTransformBBox;
  }
  return GetVisualOverflowRect();
}

void
nsFrame::CheckInvalidateSizeChange(nsHTMLReflowMetrics& aNewDesiredSize)
{
  nsIFrame::CheckInvalidateSizeChange(mRect, GetVisualOverflowRect(),
      nsSize(aNewDesiredSize.width, aNewDesiredSize.height));
}

static void
InvalidateRectForFrameSizeChange(nsIFrame* aFrame, const nsRect& aRect)
{
  nsStyleContext *bgSC;
  if (!nsCSSRendering::FindBackground(aFrame->PresContext(), aFrame, &bgSC)) {
    nsIFrame* rootFrame =
      aFrame->PresContext()->PresShell()->FrameManager()->GetRootFrame();
    rootFrame->Invalidate(nsRect(nsPoint(0, 0), rootFrame->GetSize()));
  }

  aFrame->Invalidate(aRect);
}

void
nsIFrame::CheckInvalidateSizeChange(const nsRect& aOldRect,
                                    const nsRect& aOldVisualOverflowRect,
                                    const nsSize& aNewDesiredSize)
{
  if (aNewDesiredSize == aOldRect.Size())
    return;

  // Below, we invalidate the old frame area (or, in the case of
  // outline, combined area) if the outline, border or background
  // settings indicate that something other than the difference
  // between the old and new areas needs to be painted. We are
  // assuming that the difference between the old and new areas will
  // be invalidated by some other means. That also means invalidating
  // the old frame area is the same as invalidating the new frame area
  // (since in either case the UNION of old and new areas will be
  // invalidated)

  // We use InvalidateRectForFrameSizeChange throughout this method, even
  // though root-invalidation is technically only needed in the case where
  // layer.RenderingMightDependOnFrameSize().  This allows us to simplify the
  // code somewhat and return immediately after invalidation in the earlier
  // cases.

  // Invalidate the entire old frame+outline if the frame has an outline
  PRBool anyOutlineOrEffects;
  nsRect r = ComputeOutlineAndEffectsRect(this, &anyOutlineOrEffects,
                                          aOldVisualOverflowRect,
                                          aNewDesiredSize,
                                          PR_FALSE);
  if (anyOutlineOrEffects) {
    r.UnionRect(aOldVisualOverflowRect, r);
    InvalidateRectForFrameSizeChange(this, r);
    return;
  }

  // Invalidate the old frame border box if the frame has borders. Those
  // borders may be moving.
  const nsStyleBorder* border = GetStyleBorder();
  NS_FOR_CSS_SIDES(side) {
    if (border->GetActualBorderWidth(side) != 0) {
      if ((side == NS_SIDE_LEFT || side == NS_SIDE_TOP) &&
          !nsLayoutUtils::HasNonZeroCornerOnSide(border->mBorderRadius, side) &&
          !border->GetBorderImage() &&
          border->GetBorderStyle(side) == NS_STYLE_BORDER_STYLE_SOLID) {
        // We also need to be sure that the bottom-left or top-right
        // corner is simple. For example, if the bottom or right border
        // has a different color, we would need to invalidate the corner
        // area. But that's OK because if there is a right or bottom border,
        // we'll invalidate the entire border-box here anyway.
        continue;
      }
      InvalidateRectForFrameSizeChange(this, nsRect(0, 0, aOldRect.width, aOldRect.height));
      return;
    }
  }

  const nsStyleBackground *bg = GetStyleBackground();
  if (!bg->IsTransparent()) {
    // Invalidate the old frame background if the frame has a background
    // whose position depends on the size of the frame
    NS_FOR_VISIBLE_BACKGROUND_LAYERS_BACK_TO_FRONT(i, bg) {
      const nsStyleBackground::Layer &layer = bg->mLayers[i];
      if (layer.RenderingMightDependOnFrameSize()) {
        InvalidateRectForFrameSizeChange(this, nsRect(0, 0, aOldRect.width, aOldRect.height));
        return;
      }
    }

    // Invalidate the old frame background if the frame has a background
    // that is being clipped by border-radius, since the old or new area
    // clipped off by the radius is not necessarily in the area that has
    // already been invalidated (even if only the top-left corner has a
    // border radius).
    if (nsLayoutUtils::HasNonZeroCorner(border->mBorderRadius)) {
      InvalidateRectForFrameSizeChange(this, nsRect(0, 0, aOldRect.width, aOldRect.height));
      return;
    }
  }
}

// Define the MAX_FRAME_DEPTH to be the ContentSink's MAX_REFLOW_DEPTH plus
// 4 for the frames above the document's frames: 
//  the Viewport, GFXScroll, ScrollPort, and Canvas
#define MAX_FRAME_DEPTH (MAX_REFLOW_DEPTH+4)

PRBool
nsFrame::IsFrameTreeTooDeep(const nsHTMLReflowState& aReflowState,
                            nsHTMLReflowMetrics& aMetrics,
                            nsReflowStatus& aStatus)
{
  if (aReflowState.mReflowDepth >  MAX_FRAME_DEPTH) {
    NS_WARNING("frame tree too deep; setting zero size and returning");
    mState |= NS_FRAME_TOO_DEEP_IN_FRAME_TREE;
    ClearOverflowRects();
    aMetrics.width = 0;
    aMetrics.height = 0;
    aMetrics.ascent = 0;
    aMetrics.mCarriedOutBottomMargin.Zero();
    aMetrics.mOverflowAreas.Clear();

    if (GetNextInFlow()) {
      // Reflow depth might vary between reflows, so we might have
      // successfully reflowed and split this frame before.  If so, we
      // shouldn't delete its continuations.
      aStatus = NS_FRAME_NOT_COMPLETE;
    } else {
      aStatus = NS_FRAME_COMPLETE;
    }

    return PR_TRUE;
  }
  mState &= ~NS_FRAME_TOO_DEEP_IN_FRAME_TREE;
  return PR_FALSE;
}

/* virtual */ PRBool nsFrame::IsContainingBlock() const
{
  const nsStyleDisplay* display = GetStyleDisplay();

  // Absolute positioning causes |display->mDisplay| to be set to block,
  // if needed.
  return display->mDisplay == NS_STYLE_DISPLAY_BLOCK || 
         display->mDisplay == NS_STYLE_DISPLAY_INLINE_BLOCK || 
         display->mDisplay == NS_STYLE_DISPLAY_LIST_ITEM ||
         display->mDisplay == NS_STYLE_DISPLAY_TABLE_CELL;
}

#ifdef NS_DEBUG

PRInt32 nsFrame::ContentIndexInContainer(const nsIFrame* aFrame)
{
  PRInt32 result = -1;

  nsIContent* content = aFrame->GetContent();
  if (content) {
    nsIContent* parentContent = content->GetParent();
    if (parentContent) {
      result = parentContent->IndexOf(content);
    }
  }

  return result;
}

/**
 * List a frame tree to stdout. Meant to be called from gdb.
 */
void
DebugListFrameTree(nsIFrame* aFrame)
{
  ((nsFrame*)aFrame)->List(stdout, 0);
}


// Debugging
NS_IMETHODIMP
nsFrame::List(FILE* out, PRInt32 aIndent) const
{
  IndentBy(out, aIndent);
  ListTag(out);
#ifdef DEBUG_waterson
  fprintf(out, " [parent=%p]", static_cast<void*>(mParent));
#endif
  if (HasView()) {
    fprintf(out, " [view=%p]", static_cast<void*>(GetView()));
  }
  fprintf(out, " {%d,%d,%d,%d}", mRect.x, mRect.y, mRect.width, mRect.height);
  if (0 != mState) {
    fprintf(out, " [state=%016llx]", mState);
  }
  nsIFrame* prevInFlow = GetPrevInFlow();
  nsIFrame* nextInFlow = GetNextInFlow();
  if (nsnull != prevInFlow) {
    fprintf(out, " prev-in-flow=%p", static_cast<void*>(prevInFlow));
  }
  if (nsnull != nextInFlow) {
    fprintf(out, " next-in-flow=%p", static_cast<void*>(nextInFlow));
  }
  fprintf(out, " [content=%p]", static_cast<void*>(mContent));
  nsFrame* f = const_cast<nsFrame*>(this);
  if (f->HasOverflowAreas()) {
    nsRect overflowArea = f->GetVisualOverflowRect();
    fprintf(out, " [vis-overflow=%d,%d,%d,%d]", overflowArea.x, overflowArea.y,
            overflowArea.width, overflowArea.height);
    overflowArea = f->GetScrollableOverflowRect();
    fprintf(out, " [scr-overflow=%d,%d,%d,%d]", overflowArea.x, overflowArea.y,
            overflowArea.width, overflowArea.height);
  }
  fprintf(out, " [sc=%p]", static_cast<void*>(mStyleContext));
  fputs("\n", out);
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Frame"), aResult);
}

NS_IMETHODIMP_(nsFrameState)
nsFrame::GetDebugStateBits() const
{
  // We'll ignore these flags for the purposes of comparing frame state:
  //
  //   NS_FRAME_EXTERNAL_REFERENCE
  //     because this is set by the event state manager or the
  //     caret code when a frame is focused. Depending on whether
  //     or not the regression tests are run as the focused window
  //     will make this value vary randomly.
#define IRRELEVANT_FRAME_STATE_FLAGS NS_FRAME_EXTERNAL_REFERENCE

#define FRAME_STATE_MASK (~(IRRELEVANT_FRAME_STATE_FLAGS))

  return GetStateBits() & FRAME_STATE_MASK;
}

nsresult
nsFrame::MakeFrameName(const nsAString& aType, nsAString& aResult) const
{
  aResult = aType;
  if (mContent && !mContent->IsNodeOfType(nsINode::eTEXT)) {
    nsAutoString buf;
    mContent->Tag()->ToString(buf);
    aResult.Append(NS_LITERAL_STRING("(") + buf + NS_LITERAL_STRING(")"));
  }
  char buf[40];
  PR_snprintf(buf, sizeof(buf), "(%d)", ContentIndexInContainer(this));
  AppendASCIItoUTF16(buf, aResult);
  return NS_OK;
}

void
nsFrame::XMLQuote(nsString& aString)
{
  PRInt32 i, len = aString.Length();
  for (i = 0; i < len; i++) {
    PRUnichar ch = aString.CharAt(i);
    if (ch == '<') {
      nsAutoString tmp(NS_LITERAL_STRING("&lt;"));
      aString.Cut(i, 1);
      aString.Insert(tmp, i);
      len += 3;
      i += 3;
    }
    else if (ch == '>') {
      nsAutoString tmp(NS_LITERAL_STRING("&gt;"));
      aString.Cut(i, 1);
      aString.Insert(tmp, i);
      len += 3;
      i += 3;
    }
    else if (ch == '\"') {
      nsAutoString tmp(NS_LITERAL_STRING("&quot;"));
      aString.Cut(i, 1);
      aString.Insert(tmp, i);
      len += 5;
      i += 5;
    }
  }
}
#endif

PRBool
nsIFrame::IsVisibleForPainting(nsDisplayListBuilder* aBuilder) {
  if (!GetStyleVisibility()->IsVisible())
    return PR_FALSE;
  nsISelection* sel = aBuilder->GetBoundingSelection();
  return !sel || IsVisibleInSelection(sel);
}

PRBool
nsIFrame::IsVisibleForPainting() {
  if (!GetStyleVisibility()->IsVisible())
    return PR_FALSE;

  nsPresContext* pc = PresContext();
  if (!pc->IsRenderingOnlySelection())
    return PR_TRUE;

  nsCOMPtr<nsISelectionController> selcon(do_QueryInterface(pc->PresShell()));
  if (selcon) {
    nsCOMPtr<nsISelection> sel;
    selcon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                         getter_AddRefs(sel));
    if (sel)
      return IsVisibleInSelection(sel);
  }
  return PR_TRUE;
}

PRBool
nsIFrame::IsVisibleInSelection(nsDisplayListBuilder* aBuilder) {
  nsISelection* sel = aBuilder->GetBoundingSelection();
  return !sel || IsVisibleInSelection(sel);
}

PRBool
nsIFrame::IsVisibleOrCollapsedForPainting(nsDisplayListBuilder* aBuilder) {
  if (!GetStyleVisibility()->IsVisibleOrCollapsed())
    return PR_FALSE;
  nsISelection* sel = aBuilder->GetBoundingSelection();
  return !sel || IsVisibleInSelection(sel);
}

PRBool
nsIFrame::IsVisibleInSelection(nsISelection* aSelection)
{
  if ((mState & NS_FRAME_SELECTED_CONTENT) == NS_FRAME_SELECTED_CONTENT)
    return PR_TRUE;
  
  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(mContent));
  PRBool vis;
  nsresult rv = aSelection->ContainsNode(node, PR_TRUE, &vis);
  return NS_FAILED(rv) || vis;
}

/* virtual */ PRBool
nsFrame::IsEmpty()
{
  return PR_FALSE;
}

PRBool
nsIFrame::CachedIsEmpty()
{
  NS_PRECONDITION(!(GetStateBits() & NS_FRAME_IS_DIRTY),
                  "Must only be called on reflowed lines");
  return IsEmpty();
}

/* virtual */ PRBool
nsFrame::IsSelfEmpty()
{
  return PR_FALSE;
}

NS_IMETHODIMP
nsFrame::GetSelectionController(nsPresContext *aPresContext, nsISelectionController **aSelCon)
{
  if (!aPresContext || !aSelCon)
    return NS_ERROR_INVALID_ARG;

  nsIFrame *frame = this;
  while (frame && (frame->GetStateBits() & NS_FRAME_INDEPENDENT_SELECTION)) {
    nsITextControlFrame *tcf = do_QueryFrame(frame);
    if (tcf) {
      return tcf->GetOwnedSelectionController(aSelCon);
    }
    frame = frame->GetParent();
  }

  return CallQueryInterface(aPresContext->GetPresShell(), aSelCon);
}

already_AddRefed<nsFrameSelection>
nsIFrame::GetFrameSelection()
{
  nsFrameSelection* fs =
    const_cast<nsFrameSelection*>(GetConstFrameSelection());
  NS_IF_ADDREF(fs);
  return fs;
}

const nsFrameSelection*
nsIFrame::GetConstFrameSelection()
{
  nsIFrame *frame = this;
  while (frame && (frame->GetStateBits() & NS_FRAME_INDEPENDENT_SELECTION)) {
    nsITextControlFrame *tcf = do_QueryFrame(frame);
    if (tcf) {
      return tcf->GetOwnedFrameSelection();
    }
    frame = frame->GetParent();
  }

  return PresContext()->PresShell()->ConstFrameSelection();
}

#ifdef NS_DEBUG
NS_IMETHODIMP
nsFrame::DumpRegressionData(nsPresContext* aPresContext, FILE* out, PRInt32 aIndent)
{
  IndentBy(out, aIndent);
  fprintf(out, "<frame va=\"%ld\" type=\"", PRUptrdiff(this));
  nsAutoString name;
  GetFrameName(name);
  XMLQuote(name);
  fputs(NS_LossyConvertUTF16toASCII(name).get(), out);
  fprintf(out, "\" state=\"%016llx\" parent=\"%ld\">\n",
          GetDebugStateBits(), PRUptrdiff(mParent));

  aIndent++;
  DumpBaseRegressionData(aPresContext, out, aIndent);
  aIndent--;

  IndentBy(out, aIndent);
  fprintf(out, "</frame>\n");

  return NS_OK;
}

void
nsFrame::DumpBaseRegressionData(nsPresContext* aPresContext, FILE* out, PRInt32 aIndent)
{
  if (GetNextSibling()) {
    IndentBy(out, aIndent);
    fprintf(out, "<next-sibling va=\"%ld\"/>\n", PRUptrdiff(GetNextSibling()));
  }

  if (HasView()) {
    IndentBy(out, aIndent);
    fprintf(out, "<view va=\"%ld\">\n", PRUptrdiff(GetView()));
    aIndent++;
    // XXX add in code to dump out view state too...
    aIndent--;
    IndentBy(out, aIndent);
    fprintf(out, "</view>\n");
  }

  IndentBy(out, aIndent);
  fprintf(out, "<bbox x=\"%d\" y=\"%d\" w=\"%d\" h=\"%d\"/>\n",
          mRect.x, mRect.y, mRect.width, mRect.height);

  // Now dump all of the children on all of the child lists
  ChildListIterator lists(this);
  for (; !lists.IsDone(); lists.Next()) {
    IndentBy(out, aIndent);
    if (lists.CurrentID() != kPrincipalList) {
      fprintf(out, "<child-list name=\"%s\">\n", mozilla::layout::ChildListName(lists.CurrentID()));
    }
    else {
      fprintf(out, "<child-list>\n");
    }
    aIndent++;
    nsFrameList::Enumerator childFrames(lists.CurrentList());
    for (; !childFrames.AtEnd(); childFrames.Next()) {
      nsIFrame* kid = childFrames.get();
      kid->DumpRegressionData(aPresContext, out, aIndent);
    }
    aIndent--;
    IndentBy(out, aIndent);
    fprintf(out, "</child-list>\n");
  }
}
#endif

void
nsIFrame::SetSelected(PRBool aSelected, SelectionType aType)
{
  NS_ASSERTION(!GetPrevContinuation(),
               "Should only be called on first in flow");
  if (aType != nsISelectionController::SELECTION_NORMAL)
    return;

  // check whether style allows selection
  PRBool selectable;
  IsSelectable(&selectable, nsnull);
  if (!selectable)
    return;

  for (nsIFrame* f = this; f; f = f->GetNextContinuation()) {
    if (aSelected) {
      AddStateBits(NS_FRAME_SELECTED_CONTENT);
    } else {
      RemoveStateBits(NS_FRAME_SELECTED_CONTENT);
    }

    // Repaint this frame subtree's entire area
    InvalidateFrameSubtree();
  }
}

NS_IMETHODIMP
nsFrame::GetSelected(PRBool *aSelected) const
{
  if (!aSelected )
    return NS_ERROR_NULL_POINTER;
  *aSelected = !!(mState & NS_FRAME_SELECTED_CONTENT);
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::GetPointFromOffset(PRInt32 inOffset, nsPoint* outPoint)
{
  NS_PRECONDITION(outPoint != nsnull, "Null parameter");
  nsRect contentRect = GetContentRect() - GetPosition();
  nsPoint pt = contentRect.TopLeft();
  if (mContent)
  {
    nsIContent* newContent = mContent->GetParent();
    if (newContent){
      PRInt32 newOffset = newContent->IndexOf(mContent);

      PRBool isRTL = (NS_GET_EMBEDDING_LEVEL(this) & 1) == 1;
      if ((!isRTL && inOffset > newOffset) ||
          (isRTL && inOffset <= newOffset)) {
        pt = contentRect.TopRight();
      }
    }
  }
  *outPoint = pt;
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::GetChildFrameContainingOffset(PRInt32 inContentOffset, PRBool inHint, PRInt32* outFrameContentOffset, nsIFrame **outChildFrame)
{
  NS_PRECONDITION(outChildFrame && outFrameContentOffset, "Null parameter");
  *outFrameContentOffset = (PRInt32)inHint;
  //the best frame to reflect any given offset would be a visible frame if possible
  //i.e. we are looking for a valid frame to place the blinking caret 
  nsRect rect = GetRect();
  if (!rect.width || !rect.height)
  {
    //if we have a 0 width or height then lets look for another frame that possibly has
    //the same content.  If we have no frames in flow then just let us return 'this' frame
    nsIFrame* nextFlow = GetNextInFlow();
    if (nextFlow)
      return nextFlow->GetChildFrameContainingOffset(inContentOffset, inHint, outFrameContentOffset, outChildFrame);
  }
  *outChildFrame = this;
  return NS_OK;
}

//
// What I've pieced together about this routine:
// Starting with a block frame (from which a line frame can be gotten)
// and a line number, drill down and get the first/last selectable
// frame on that line, depending on aPos->mDirection.
// aOutSideLimit != 0 means ignore aLineStart, instead work from
// the end (if > 0) or beginning (if < 0).
//
nsresult
nsFrame::GetNextPrevLineFromeBlockFrame(nsPresContext* aPresContext,
                                        nsPeekOffsetStruct *aPos,
                                        nsIFrame *aBlockFrame, 
                                        PRInt32 aLineStart, 
                                        PRInt8 aOutSideLimit
                                        )
{
  //magic numbers aLineStart will be -1 for end of block 0 will be start of block
  if (!aBlockFrame || !aPos)
    return NS_ERROR_NULL_POINTER;

  aPos->mResultFrame = nsnull;
  aPos->mResultContent = nsnull;
  aPos->mAttachForward = (aPos->mDirection == eDirNext);

  nsAutoLineIterator it = aBlockFrame->GetLineIterator();
  if (!it)
    return NS_ERROR_FAILURE;
  PRInt32 searchingLine = aLineStart;
  PRInt32 countLines = it->GetNumLines();
  if (aOutSideLimit > 0) //start at end
    searchingLine = countLines;
  else if (aOutSideLimit <0)//start at beginning
    searchingLine = -1;//"next" will be 0  
  else 
    if ((aPos->mDirection == eDirPrevious && searchingLine == 0) || 
       (aPos->mDirection == eDirNext && searchingLine >= (countLines -1) )){
      //we need to jump to new block frame.
           return NS_ERROR_FAILURE;
    }
  PRInt32 lineFrameCount;
  nsIFrame *resultFrame = nsnull;
  nsIFrame *farStoppingFrame = nsnull; //we keep searching until we find a "this" frame then we go to next line
  nsIFrame *nearStoppingFrame = nsnull; //if we are backing up from edge, stop here
  nsIFrame *firstFrame;
  nsIFrame *lastFrame;
  nsRect  rect;
  PRBool isBeforeFirstFrame, isAfterLastFrame;
  PRBool found = PR_FALSE;

  nsresult result = NS_OK;
  while (!found)
  {
    if (aPos->mDirection == eDirPrevious)
      searchingLine --;
    else
      searchingLine ++;
    if ((aPos->mDirection == eDirPrevious && searchingLine < 0) || 
       (aPos->mDirection == eDirNext && searchingLine >= countLines ))
    {
      //we need to jump to new block frame.
      return NS_ERROR_FAILURE;
    }
    PRUint32 lineFlags;
    result = it->GetLine(searchingLine, &firstFrame, &lineFrameCount,
                         rect, &lineFlags);
    if (!lineFrameCount) 
      continue;
    if (NS_SUCCEEDED(result)){
      lastFrame = firstFrame;
      for (;lineFrameCount > 1;lineFrameCount --){
        //result = lastFrame->GetNextSibling(&lastFrame, searchingLine);
        result = it->GetNextSiblingOnLine(lastFrame, searchingLine);
        if (NS_FAILED(result) || !lastFrame){
          NS_ERROR("GetLine promised more frames than could be found");
          return NS_ERROR_FAILURE;
        }
      }
      GetLastLeaf(aPresContext, &lastFrame);

      if (aPos->mDirection == eDirNext){
        nearStoppingFrame = firstFrame;
        farStoppingFrame = lastFrame;
      }
      else{
        nearStoppingFrame = lastFrame;
        farStoppingFrame = firstFrame;
      }
      nsPoint offset;
      nsIView * view; //used for call of get offset from view
      aBlockFrame->GetOffsetFromView(offset,&view);
      nscoord newDesiredX  = aPos->mDesiredX - offset.x;//get desired x into blockframe coordinates!
      result = it->FindFrameAt(searchingLine, newDesiredX, &resultFrame, &isBeforeFirstFrame, &isAfterLastFrame);
      if(NS_FAILED(result))
        continue;
    }

    if (NS_SUCCEEDED(result) && resultFrame)
    {
      //check to see if this is ANOTHER blockframe inside the other one if so then call into its lines
      nsAutoLineIterator newIt = resultFrame->GetLineIterator();
      if (newIt)
      {
        aPos->mResultFrame = resultFrame;
        return NS_OK;
      }
      //resultFrame is not a block frame
      result = NS_ERROR_FAILURE;

      nsCOMPtr<nsIFrameEnumerator> frameTraversal;
      result = NS_NewFrameTraversal(getter_AddRefs(frameTraversal),
                                    aPresContext, resultFrame,
                                    ePostOrder,
                                    PR_FALSE, // aVisual
                                    aPos->mScrollViewStop,
                                    PR_FALSE  // aFollowOOFs
                                    );
      if (NS_FAILED(result))
        return result;
      nsIFrame *storeOldResultFrame = resultFrame;
      while ( !found ){
        nsPoint point;
        point.x = aPos->mDesiredX;

        nsRect tempRect = resultFrame->GetRect();
        nsPoint offset;
        nsIView * view; //used for call of get offset from view
        result = resultFrame->GetOffsetFromView(offset, &view);
        if (NS_FAILED(result))
          return result;
        point.y = tempRect.height + offset.y;

        //special check. if we allow non-text selection then we can allow a hit location to fall before a table. 
        //otherwise there is no way to get and click signal to fall before a table (it being a line iterator itself)
        nsIPresShell *shell = aPresContext->GetPresShell();
        if (!shell)
          return NS_ERROR_FAILURE;
        PRInt16 isEditor = shell->GetSelectionFlags();
        isEditor = isEditor == nsISelectionDisplay::DISPLAY_ALL;
        if ( isEditor )
        {
          if (resultFrame->GetType() == nsGkAtoms::tableOuterFrame)
          {
            if (((point.x - offset.x + tempRect.x)<0) ||  ((point.x - offset.x+ tempRect.x)>tempRect.width))//off left/right side
            {
              nsIContent* content = resultFrame->GetContent();
              if (content)
              {
                nsIContent* parent = content->GetParent();
                if (parent)
                {
                  aPos->mResultContent = parent;
                  aPos->mContentOffset = parent->IndexOf(content);
                  aPos->mAttachForward = PR_FALSE;
                  if ((point.x - offset.x+ tempRect.x)>tempRect.width)
                  {
                    aPos->mContentOffset++;//go to end of this frame
                    aPos->mAttachForward = PR_TRUE;
                  }
                  //result frame is the result frames parent.
                  aPos->mResultFrame = resultFrame->GetParent();
                  return NS_POSITION_BEFORE_TABLE;
                }
              }
            }
          }
        }

        if (!resultFrame->HasView())
        {
          nsIView* view;
          nsPoint offset;
          resultFrame->GetOffsetFromView(offset, &view);
          ContentOffsets offsets =
              resultFrame->GetContentOffsetsFromPoint(point - offset);
          aPos->mResultContent = offsets.content;
          aPos->mContentOffset = offsets.offset;
          aPos->mAttachForward = offsets.associateWithNext;
          if (offsets.content)
          {
            PRBool selectable;
            resultFrame->IsSelectable(&selectable, nsnull);
            if (selectable)
            {
              found = PR_TRUE;
              break;
            }
          }
        }

        if (aPos->mDirection == eDirPrevious && (resultFrame == farStoppingFrame))
          break;
        if (aPos->mDirection == eDirNext && (resultFrame == nearStoppingFrame))
          break;
        //always try previous on THAT line if that fails go the other way
        frameTraversal->Prev();
        resultFrame = frameTraversal->CurrentItem();
        if (!resultFrame)
          return NS_ERROR_FAILURE;
      }

      if (!found){
        resultFrame = storeOldResultFrame;
        result = NS_NewFrameTraversal(getter_AddRefs(frameTraversal),
                                      aPresContext, resultFrame,
                                      eLeaf,
                                      PR_FALSE, // aVisual
                                      aPos->mScrollViewStop,
                                      PR_FALSE  // aFollowOOFs
                                      );
      }
      while ( !found ){
        nsPoint point(aPos->mDesiredX, 0);
        nsIView* view;
        nsPoint offset;
        resultFrame->GetOffsetFromView(offset, &view);
        ContentOffsets offsets =
            resultFrame->GetContentOffsetsFromPoint(point - offset);
        aPos->mResultContent = offsets.content;
        aPos->mContentOffset = offsets.offset;
        aPos->mAttachForward = offsets.associateWithNext;
        if (offsets.content)
        {
          PRBool selectable;
          resultFrame->IsSelectable(&selectable, nsnull);
          if (selectable)
          {
            found = PR_TRUE;
            if (resultFrame == farStoppingFrame)
              aPos->mAttachForward = PR_FALSE;
            else
              aPos->mAttachForward = PR_TRUE;
            break;
          }
        }
        if (aPos->mDirection == eDirPrevious && (resultFrame == nearStoppingFrame))
          break;
        if (aPos->mDirection == eDirNext && (resultFrame == farStoppingFrame))
          break;
        //previous didnt work now we try "next"
        frameTraversal->Next();
        nsIFrame *tempFrame = frameTraversal->CurrentItem();
        if (!tempFrame)
          break;
        resultFrame = tempFrame;
      }
      aPos->mResultFrame = resultFrame;
    }
    else {
        //we need to jump to new block frame.
      aPos->mAmount = eSelectLine;
      aPos->mStartOffset = 0;
      aPos->mAttachForward = !(aPos->mDirection == eDirNext);
      if (aPos->mDirection == eDirPrevious)
        aPos->mStartOffset = -1;//start from end
     return aBlockFrame->PeekOffset(aPos);
    }
  }
  return NS_OK;
}

nsPeekOffsetStruct nsIFrame::GetExtremeCaretPosition(PRBool aStart)
{
  nsPeekOffsetStruct result;

  FrameTarget targetFrame = DrillDownToSelectionFrame(this, !aStart);
  FrameContentRange range = GetRangeForFrame(targetFrame.frame);
  result.mResultContent = range.content;
  result.mContentOffset = aStart ? range.start : range.end;
  result.mAttachForward = (result.mContentOffset == range.start);
  return result;
}

// Find the first (or last) descendant of the given frame
// which is either a block frame or a BRFrame.
static nsContentAndOffset
FindBlockFrameOrBR(nsIFrame* aFrame, nsDirection aDirection)
{
  nsContentAndOffset result;
  result.mContent =  nsnull;
  result.mOffset = 0;

  if (aFrame->IsGeneratedContentFrame())
    return result;

  // Treat form controls as inline leaves
  // XXX we really need a way to determine whether a frame is inline-level
  nsIFormControlFrame* fcf = do_QueryFrame(aFrame);
  if (fcf)
    return result;
  
  // Check the frame itself
  // Fall through "special" block frames because their mContent is the content
  // of the inline frames they were created from. The first/last child of
  // such frames is the real block frame we're looking for.
  if ((nsLayoutUtils::GetAsBlock(aFrame) && !(aFrame->GetStateBits() & NS_FRAME_IS_SPECIAL)) ||
      aFrame->GetType() == nsGkAtoms::brFrame) {
    nsIContent* content = aFrame->GetContent();
    result.mContent = content->GetParent();
    // In some cases (bug 310589, bug 370174) we end up here with a null content.
    // This probably shouldn't ever happen, but since it sometimes does, we want
    // to avoid crashing here.
    NS_ASSERTION(result.mContent, "Unexpected orphan content");
    if (result.mContent)
      result.mOffset = result.mContent->IndexOf(content) + 
        (aDirection == eDirPrevious ? 1 : 0);
    return result;
  }

  // If this is a preformatted text frame, see if it ends with a newline
  if (aFrame->HasTerminalNewline() &&
      aFrame->GetStyleContext()->GetStyleText()->NewlineIsSignificant()) {
    PRInt32 startOffset, endOffset;
    aFrame->GetOffsets(startOffset, endOffset);
    result.mContent = aFrame->GetContent();
    result.mOffset = endOffset - (aDirection == eDirPrevious ? 0 : 1);
    return result;
  }

  // Iterate over children and call ourselves recursively
  if (aDirection == eDirPrevious) {
    nsIFrame* child = aFrame->GetLastChild(nsIFrame::kPrincipalList);
    while(child && !result.mContent) {
      result = FindBlockFrameOrBR(child, aDirection);
      child = child->GetPrevSibling();
    }
  } else { // eDirNext
    nsIFrame* child = aFrame->GetFirstPrincipalChild();
    while(child && !result.mContent) {
      result = FindBlockFrameOrBR(child, aDirection);
      child = child->GetNextSibling();
    }
  }
  return result;
}

nsresult
nsIFrame::PeekOffsetParagraph(nsPeekOffsetStruct *aPos)
{
  nsIFrame* frame = this;
  nsContentAndOffset blockFrameOrBR;
  blockFrameOrBR.mContent = nsnull;
  PRBool reachedBlockAncestor = PR_FALSE;

  // Go through containing frames until reaching a block frame.
  // In each step, search the previous (or next) siblings for the closest
  // "stop frame" (a block frame or a BRFrame).
  // If found, set it to be the selection boundray and abort.
  
  if (aPos->mDirection == eDirPrevious) {
    while (!reachedBlockAncestor) {
      nsIFrame* parent = frame->GetParent();
      // Treat a frame associated with the root content as if it were a block frame.
      if (!frame->mContent || !frame->mContent->GetParent()) {
        reachedBlockAncestor = PR_TRUE;
        break;
      }
      nsIFrame* sibling = frame->GetPrevSibling();
      while (sibling && !blockFrameOrBR.mContent) {
        blockFrameOrBR = FindBlockFrameOrBR(sibling, eDirPrevious);
        sibling = sibling->GetPrevSibling();
      }
      if (blockFrameOrBR.mContent) {
        aPos->mResultContent = blockFrameOrBR.mContent;
        aPos->mContentOffset = blockFrameOrBR.mOffset;
        break;
      }
      frame = parent;
      reachedBlockAncestor = (nsLayoutUtils::GetAsBlock(frame) != nsnull);
    }
    if (reachedBlockAncestor) { // no "stop frame" found
      aPos->mResultContent = frame->GetContent();
      aPos->mContentOffset = 0;
    }
  } else { // eDirNext
    while (!reachedBlockAncestor) {
      nsIFrame* parent = frame->GetParent();
      // Treat a frame associated with the root content as if it were a block frame.
      if (!frame->mContent || !frame->mContent->GetParent()) {
        reachedBlockAncestor = PR_TRUE;
        break;
      }
      nsIFrame* sibling = frame;
      while (sibling && !blockFrameOrBR.mContent) {
        blockFrameOrBR = FindBlockFrameOrBR(sibling, eDirNext);
        sibling = sibling->GetNextSibling();
      }
      if (blockFrameOrBR.mContent) {
        aPos->mResultContent = blockFrameOrBR.mContent;
        aPos->mContentOffset = blockFrameOrBR.mOffset;
        break;
      }
      frame = parent;
      reachedBlockAncestor = (nsLayoutUtils::GetAsBlock(frame) != nsnull);
    }
    if (reachedBlockAncestor) { // no "stop frame" found
      aPos->mResultContent = frame->GetContent();
      if (aPos->mResultContent)
        aPos->mContentOffset = aPos->mResultContent->GetChildCount();
    }
  }
  return NS_OK;
}

// Determine movement direction relative to frame
static PRBool IsMovingInFrameDirection(nsIFrame* frame, nsDirection aDirection, PRBool aVisual)
{
  PRBool isReverseDirection = aVisual ?
    (NS_GET_EMBEDDING_LEVEL(frame) & 1) != (NS_GET_BASE_LEVEL(frame) & 1) : PR_FALSE;
  return aDirection == (isReverseDirection ? eDirPrevious : eDirNext);
}

NS_IMETHODIMP
nsIFrame::PeekOffset(nsPeekOffsetStruct* aPos)
{
  if (!aPos)
    return NS_ERROR_NULL_POINTER;
  nsresult result = NS_ERROR_FAILURE;

  if (mState & NS_FRAME_IS_DIRTY)
    return NS_ERROR_UNEXPECTED;

  // Translate content offset to be relative to frame
  FrameContentRange range = GetRangeForFrame(this);
  PRInt32 offset = aPos->mStartOffset - range.start;
  nsIFrame* current = this;
  
  switch (aPos->mAmount) {
    case eSelectCharacter:
    case eSelectCluster:
    {
      PRBool eatingNonRenderableWS = PR_FALSE;
      PRBool done = PR_FALSE;
      PRBool jumpedLine = PR_FALSE;
      
      while (!done) {
        PRBool movingInFrameDirection =
          IsMovingInFrameDirection(current, aPos->mDirection, aPos->mVisual);

        if (eatingNonRenderableWS)
          done = current->PeekOffsetNoAmount(movingInFrameDirection, &offset); 
        else
          done = current->PeekOffsetCharacter(movingInFrameDirection, &offset,
                                              aPos->mAmount == eSelectCluster);

        if (!done) {
          result =
            current->GetFrameFromDirection(aPos->mDirection, aPos->mVisual,
                                           aPos->mJumpLines, aPos->mScrollViewStop,
                                           &current, &offset, &jumpedLine);
          if (NS_FAILED(result))
            return result;

          // If we jumped lines, it's as if we found a character, but we still need
          // to eat non-renderable content on the new line.
          if (jumpedLine)
            eatingNonRenderableWS = PR_TRUE;
        }
      }

      // Set outputs
      range = GetRangeForFrame(current);
      aPos->mResultFrame = current;
      aPos->mResultContent = range.content;
      // Output offset is relative to content, not frame
      aPos->mContentOffset = offset < 0 ? range.end : range.start + offset;
      // If we're dealing with a text frame and moving backward positions us at
      // the end of that line, decrease the offset by one to make sure that
      // we're placed before the linefeed character on the previous line.
      if (offset < 0 && jumpedLine &&
          aPos->mDirection == eDirPrevious &&
          current->GetStyleText()->NewlineIsSignificant() &&
          current->HasTerminalNewline()) {
        --aPos->mContentOffset;
      }
      
      break;
    }
    case eSelectWordNoSpace:
      // eSelectWordNoSpace means that we should not be eating any whitespace when
      // moving to the adjacent word.  This means that we should set aPos->
      // mWordMovementType to eEndWord if we're moving forwards, and to eStartWord
      // if we're moving backwards.
      if (aPos->mDirection == eDirPrevious) {
        aPos->mWordMovementType = eStartWord;
      } else {
        aPos->mWordMovementType = eEndWord;
      }
      // Intentionally fall through the eSelectWord case.
    case eSelectWord:
    {
      // wordSelectEatSpace means "are we looking for a boundary between whitespace
      // and non-whitespace (in the direction we're moving in)".
      // It is true when moving forward and looking for a beginning of a word, or
      // when moving backwards and looking for an end of a word.
      PRBool wordSelectEatSpace;
      if (aPos->mWordMovementType != eDefaultBehavior) {
        // aPos->mWordMovementType possible values:
        //       eEndWord: eat the space if we're moving backwards
        //       eStartWord: eat the space if we're moving forwards
        wordSelectEatSpace = ((aPos->mWordMovementType == eEndWord) == (aPos->mDirection == eDirPrevious));
      }
      else {
        // Use the hidden preference which is based on operating system behavior.
        // This pref only affects whether moving forward by word should go to the end of this word or start of the next word.
        // When going backwards, the start of the word is always used, on every operating system.
        wordSelectEatSpace = aPos->mDirection == eDirNext &&
          Preferences::GetBool("layout.word_select.eat_space_to_next_word");
      }
      
      // mSawBeforeType means "we already saw characters of the type
      // before the boundary we're looking for". Examples:
      // 1. If we're moving forward, looking for a word beginning (i.e. a boundary
      //    between whitespace and non-whitespace), then eatingWS==PR_TRUE means
      //    "we already saw some whitespace".
      // 2. If we're moving backward, looking for a word beginning (i.e. a boundary
      //    between non-whitespace and whitespace), then eatingWS==PR_TRUE means
      //    "we already saw some non-whitespace".
      PeekWordState state;
      PRInt32 offsetAdjustment = 0;
      PRBool done = PR_FALSE;
      while (!done) {
        PRBool movingInFrameDirection =
          IsMovingInFrameDirection(current, aPos->mDirection, aPos->mVisual);
        
        done = current->PeekOffsetWord(movingInFrameDirection, wordSelectEatSpace,
                                       aPos->mIsKeyboardSelect, &offset, &state);
        
        if (!done) {
          nsIFrame* nextFrame;
          PRInt32 nextFrameOffset;
          PRBool jumpedLine;
          result =
            current->GetFrameFromDirection(aPos->mDirection, aPos->mVisual,
                                           aPos->mJumpLines, aPos->mScrollViewStop,
                                           &nextFrame, &nextFrameOffset, &jumpedLine);
          // We can't jump lines if we're looking for whitespace following
          // non-whitespace, and we already encountered non-whitespace.
          if (NS_FAILED(result) ||
              (jumpedLine && !wordSelectEatSpace && state.mSawBeforeType)) {
            done = PR_TRUE;
            // If we've crossed the line boundary, check to make sure that we
            // have not consumed a trailing newline as whitesapce if it's significant.
            if (jumpedLine && wordSelectEatSpace &&
                current->HasTerminalNewline() &&
                current->GetStyleText()->NewlineIsSignificant()) {
              offsetAdjustment = -1;
            }
          } else {
            if (jumpedLine) {
              state.mContext.Truncate();
            }
            current = nextFrame;
            offset = nextFrameOffset;
            // Jumping a line is equivalent to encountering whitespace
            if (wordSelectEatSpace && jumpedLine)
              state.SetSawBeforeType();
          }
        }
      }
      
      // Set outputs
      range = GetRangeForFrame(current);
      aPos->mResultFrame = current;
      aPos->mResultContent = range.content;
      // Output offset is relative to content, not frame
      aPos->mContentOffset = (offset < 0 ? range.end : range.start + offset) + offsetAdjustment;
      break;
    }
    case eSelectLine :
    {
      nsAutoLineIterator iter;
      nsIFrame *blockFrame = this;

      while (NS_FAILED(result)){
        PRInt32 thisLine = nsFrame::GetLineNumber(blockFrame, aPos->mScrollViewStop, &blockFrame);
        if (thisLine < 0) 
          return  NS_ERROR_FAILURE;
        iter = blockFrame->GetLineIterator();
        NS_ASSERTION(iter, "GetLineNumber() succeeded but no block frame?");
        result = NS_OK;

        int edgeCase = 0;//no edge case. this should look at thisLine
        
        PRBool doneLooping = PR_FALSE;//tells us when no more block frames hit.
        //this part will find a frame or a block frame. if it's a block frame
        //it will "drill down" to find a viable frame or it will return an error.
        nsIFrame *lastFrame = this;
        do {
          result = nsFrame::GetNextPrevLineFromeBlockFrame(PresContext(),
                                                           aPos, 
                                                           blockFrame, 
                                                           thisLine, 
                                                           edgeCase //start from thisLine
            );
          if (NS_SUCCEEDED(result) && (!aPos->mResultFrame || aPos->mResultFrame == lastFrame))//we came back to same spot! keep going
          {
            aPos->mResultFrame = nsnull;
            if (aPos->mDirection == eDirPrevious)
              thisLine--;
            else
              thisLine++;
          }
          else //if failure or success with different frame.
            doneLooping = PR_TRUE; //do not continue with while loop

          lastFrame = aPos->mResultFrame; //set last frame 

          if (NS_SUCCEEDED(result) && aPos->mResultFrame 
            && blockFrame != aPos->mResultFrame)// make sure block element is not the same as the one we had before
          {
/* SPECIAL CHECK FOR TABLE NAVIGATION
  tables need to navigate also and the frame that supports it is nsTableRowGroupFrame which is INSIDE
  nsTableOuterFrame.  if we have stumbled onto an nsTableOuter we need to drill into nsTableRowGroup
  if we hit a header or footer that's ok just go into them,
*/
            PRBool searchTableBool = PR_FALSE;
            if (aPos->mResultFrame->GetType() == nsGkAtoms::tableOuterFrame ||
                aPos->mResultFrame->GetType() == nsGkAtoms::tableCellFrame)
            {
              nsIFrame *frame = aPos->mResultFrame->GetFirstPrincipalChild();
              //got the table frame now
              while(frame) //ok time to drill down to find iterator
              {
                iter = frame->GetLineIterator();
                if (iter)
                {
                  aPos->mResultFrame = frame;
                  searchTableBool = PR_TRUE;
                  result = NS_OK;
                  break; //while(frame)
                }
                result = NS_ERROR_FAILURE;
                frame = frame->GetFirstPrincipalChild();
              }
            }

            if (!searchTableBool) {
              iter = aPos->mResultFrame->GetLineIterator();
              result = iter ? NS_OK : NS_ERROR_FAILURE;
            }
            if (NS_SUCCEEDED(result) && iter)//we've struck another block element!
            {
              doneLooping = PR_FALSE;
              if (aPos->mDirection == eDirPrevious)
                edgeCase = 1;//far edge, search from end backwards
              else
                edgeCase = -1;//near edge search from beginning onwards
              thisLine=0;//this line means nothing now.
              //everything else means something so keep looking "inside" the block
              blockFrame = aPos->mResultFrame;

            }
            else
            {
              result = NS_OK;//THIS is to mean that everything is ok to the containing while loop
              break;
            }
          }
        } while (!doneLooping);
      }
      return result;
    }

    case eSelectParagraph:
      return PeekOffsetParagraph(aPos);

    case eSelectBeginLine:
    case eSelectEndLine:
    {
      // Adjusted so that the caret can't get confused when content changes
      nsIFrame* blockFrame = AdjustFrameForSelectionStyles(this);
      PRInt32 thisLine = nsFrame::GetLineNumber(blockFrame, aPos->mScrollViewStop, &blockFrame);
      if (thisLine < 0)
        return NS_ERROR_FAILURE;
      nsAutoLineIterator it = blockFrame->GetLineIterator();
      NS_ASSERTION(it, "GetLineNumber() succeeded but no block frame?");

      PRInt32 lineFrameCount;
      nsIFrame *firstFrame;
      nsRect usedRect;
      PRUint32 lineFlags;
      nsIFrame* baseFrame = nsnull;
      PRBool endOfLine = (eSelectEndLine == aPos->mAmount);
      
#ifdef IBMBIDI
      if (aPos->mVisual && PresContext()->BidiEnabled()) {
        PRBool lineIsRTL = it->GetDirection();
        PRBool isReordered;
        nsIFrame *lastFrame;
        result = it->CheckLineOrder(thisLine, &isReordered, &firstFrame, &lastFrame);
        baseFrame = endOfLine ? lastFrame : firstFrame;
        if (baseFrame) {
          nsBidiLevel embeddingLevel = nsBidiPresUtils::GetFrameEmbeddingLevel(baseFrame);
          // If the direction of the frame on the edge is opposite to that of the line,
          // we'll need to drill down to its opposite end, so reverse endOfLine.
          if ((embeddingLevel & 1) == !lineIsRTL)
            endOfLine = !endOfLine;
        }
      } else
#endif
      {
        it->GetLine(thisLine, &firstFrame, &lineFrameCount, usedRect, &lineFlags);

        nsIFrame* frame = firstFrame;
        for (PRInt32 count = lineFrameCount; count;
             --count, frame = frame->GetNextSibling()) {
          if (!frame->IsGeneratedContentFrame()) {
            baseFrame = frame;
            if (!endOfLine)
              break;
          }
        }
      }
      if (!baseFrame)
        return NS_ERROR_FAILURE;
      FrameTarget targetFrame = DrillDownToSelectionFrame(baseFrame,
                                                          endOfLine);
      FrameContentRange range = GetRangeForFrame(targetFrame.frame);
      aPos->mResultContent = range.content;
      aPos->mContentOffset = endOfLine ? range.end : range.start;
      if (endOfLine && targetFrame.frame->HasTerminalNewline()) {
        // Do not position the caret after the terminating newline if we're
        // trying to move to the end of line (see bug 596506)
        --aPos->mContentOffset;
      }
      aPos->mResultFrame = targetFrame.frame;
      aPos->mAttachForward = (aPos->mContentOffset == range.start);
      if (!range.content)
        return NS_ERROR_FAILURE;
      return NS_OK;
    }

    default: 
    {
      NS_ASSERTION(PR_FALSE, "Invalid amount");
      return NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}

PRBool
nsFrame::PeekOffsetNoAmount(PRBool aForward, PRInt32* aOffset)
{
  NS_ASSERTION (aOffset && *aOffset <= 1, "aOffset out of range");
  // Sure, we can stop right here.
  return PR_TRUE;
}

PRBool
nsFrame::PeekOffsetCharacter(PRBool aForward, PRInt32* aOffset,
                             PRBool aRespectClusters)
{
  NS_ASSERTION (aOffset && *aOffset <= 1, "aOffset out of range");
  PRInt32 startOffset = *aOffset;
  // A negative offset means "end of frame", which in our case means offset 1.
  if (startOffset < 0)
    startOffset = 1;
  if (aForward == (startOffset == 0)) {
    // We're before the frame and moving forward, or after it and moving backwards:
    // skip to the other side and we're done.
    *aOffset = 1 - startOffset;
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool
nsFrame::PeekOffsetWord(PRBool aForward, PRBool aWordSelectEatSpace, PRBool aIsKeyboardSelect,
                        PRInt32* aOffset, PeekWordState* aState)
{
  NS_ASSERTION (aOffset && *aOffset <= 1, "aOffset out of range");
  PRInt32 startOffset = *aOffset;
  // This isn't text, so truncate the context
  aState->mContext.Truncate();
  if (startOffset < 0)
    startOffset = 1;
  if (aForward == (startOffset == 0)) {
    // We're before the frame and moving forward, or after it and moving backwards.
    // If we're looking for non-whitespace, we found it (without skipping this frame).
    if (!aState->mAtStart) {
      if (aState->mLastCharWasPunctuation) {
        // We're not punctuation, so this is a punctuation boundary.
        if (BreakWordBetweenPunctuation(aState, aForward, PR_FALSE, PR_FALSE, aIsKeyboardSelect))
          return PR_TRUE;
      } else {
        // This is not a punctuation boundary.
        if (aWordSelectEatSpace && aState->mSawBeforeType)
          return PR_TRUE;
      }
    }
    // Otherwise skip to the other side and note that we encountered non-whitespace.
    *aOffset = 1 - startOffset;
    aState->Update(PR_FALSE, // not punctuation
                   PR_FALSE  // not whitespace
                   );
    if (!aWordSelectEatSpace)
      aState->SetSawBeforeType();
  }
  return PR_FALSE;
}

PRBool
nsFrame::BreakWordBetweenPunctuation(const PeekWordState* aState,
                                     PRBool aForward,
                                     PRBool aPunctAfter, PRBool aWhitespaceAfter,
                                     PRBool aIsKeyboardSelect)
{
  NS_ASSERTION(aPunctAfter != aState->mLastCharWasPunctuation,
               "Call this only at punctuation boundaries");
  if (aState->mLastCharWasWhitespace) {
    // We always stop between whitespace and punctuation
    return PR_TRUE;
  }
  if (!Preferences::GetBool("layout.word_select.stop_at_punctuation")) {
    // When this pref is false, we never stop at a punctuation boundary unless
    // it's after whitespace
    return PR_FALSE;
  }
  if (!aIsKeyboardSelect) {
    // mouse caret movement (e.g. word selection) always stops at every punctuation boundary
    return PR_TRUE;
  }
  PRBool afterPunct = aForward ? aState->mLastCharWasPunctuation : aPunctAfter;
  if (!afterPunct) {
    // keyboard caret movement only stops after punctuation (in content order)
    return PR_FALSE;
  }
  // Stop only if we've seen some non-punctuation since the last whitespace;
  // don't stop after punctuation that follows whitespace.
  return aState->mSeenNonPunctuationSinceWhitespace;
}

NS_IMETHODIMP
nsFrame::CheckVisibility(nsPresContext* , PRInt32 , PRInt32 , PRBool , PRBool *, PRBool *)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


PRInt32
nsFrame::GetLineNumber(nsIFrame *aFrame, PRBool aLockScroll, nsIFrame** aContainingBlock)
{
  NS_ASSERTION(aFrame, "null aFrame");
  nsFrameManager* frameManager = aFrame->PresContext()->FrameManager();
  nsIFrame *blockFrame = aFrame;
  nsIFrame *thisBlock;
  nsAutoLineIterator it;
  nsresult result = NS_ERROR_FAILURE;
  while (NS_FAILED(result) && blockFrame)
  {
    thisBlock = blockFrame;
    if (thisBlock->GetStateBits() & NS_FRAME_OUT_OF_FLOW) {
      //if we are searching for a frame that is not in flow we will not find it. 
      //we must instead look for its placeholder
      if (thisBlock->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER) {
        // abspos continuations don't have placeholders, get the fif
        thisBlock = thisBlock->GetFirstInFlow();
      }
      thisBlock = frameManager->GetPlaceholderFrameFor(thisBlock);
      if (!thisBlock)
        return -1;
    }  
    blockFrame = thisBlock->GetParent();
    result = NS_OK;
    if (blockFrame) {
      if (aLockScroll && blockFrame->GetType() == nsGkAtoms::scrollFrame)
        return -1;
      it = blockFrame->GetLineIterator();
      if (!it)
        result = NS_ERROR_FAILURE;
    }
  }
  if (!blockFrame || !it)
    return -1;

  if (aContainingBlock)
    *aContainingBlock = blockFrame;
  return it->FindLineContaining(thisBlock);
}

nsresult
nsIFrame::GetFrameFromDirection(nsDirection aDirection, PRBool aVisual,
                                PRBool aJumpLines, PRBool aScrollViewStop, 
                                nsIFrame** aOutFrame, PRInt32* aOutOffset, PRBool* aOutJumpedLine)
{
  nsresult result;

  if (!aOutFrame || !aOutOffset || !aOutJumpedLine)
    return NS_ERROR_NULL_POINTER;
  
  nsPresContext* presContext = PresContext();
  *aOutFrame = nsnull;
  *aOutOffset = 0;
  *aOutJumpedLine = PR_FALSE;

  // Find the prev/next selectable frame
  PRBool selectable = PR_FALSE;
  nsIFrame *traversedFrame = this;
  while (!selectable) {
    nsIFrame *blockFrame;
    
    PRInt32 thisLine = nsFrame::GetLineNumber(traversedFrame, aScrollViewStop, &blockFrame);
    if (thisLine < 0)
      return NS_ERROR_FAILURE;

    nsAutoLineIterator it = blockFrame->GetLineIterator();
    NS_ASSERTION(it, "GetLineNumber() succeeded but no block frame?");

    PRBool atLineEdge;
    nsIFrame *firstFrame;
    nsIFrame *lastFrame;
#ifdef IBMBIDI
    if (aVisual && presContext->BidiEnabled()) {
      PRBool lineIsRTL = it->GetDirection();
      PRBool isReordered;
      result = it->CheckLineOrder(thisLine, &isReordered, &firstFrame, &lastFrame);
      nsIFrame** framePtr = aDirection == eDirPrevious ? &firstFrame : &lastFrame;
      if (*framePtr) {
        nsBidiLevel embeddingLevel = nsBidiPresUtils::GetFrameEmbeddingLevel(*framePtr);
        if ((((embeddingLevel & 1) && lineIsRTL) || (!(embeddingLevel & 1) && !lineIsRTL)) ==
            (aDirection == eDirPrevious)) {
          nsFrame::GetFirstLeaf(presContext, framePtr);
        } else {
          nsFrame::GetLastLeaf(presContext, framePtr);
        }
        atLineEdge = *framePtr == traversedFrame;
      } else {
        atLineEdge = PR_TRUE;
      }
    } else
#endif
    {
      nsRect  nonUsedRect;
      PRInt32 lineFrameCount;
      PRUint32 lineFlags;
      result = it->GetLine(thisLine, &firstFrame, &lineFrameCount,nonUsedRect,
                           &lineFlags);
      if (NS_FAILED(result))
        return result;

      if (aDirection == eDirPrevious) {
        nsFrame::GetFirstLeaf(presContext, &firstFrame);
        atLineEdge = firstFrame == traversedFrame;
      } else { // eDirNext
        lastFrame = firstFrame;
        for (;lineFrameCount > 1;lineFrameCount --){
          result = it->GetNextSiblingOnLine(lastFrame, thisLine);
          if (NS_FAILED(result) || !lastFrame){
            NS_ERROR("should not be reached nsFrame");
            return NS_ERROR_FAILURE;
          }
        }
        nsFrame::GetLastLeaf(presContext, &lastFrame);
        atLineEdge = lastFrame == traversedFrame;
      }
    }

    if (atLineEdge) {
      *aOutJumpedLine = PR_TRUE;
      if (!aJumpLines)
        return NS_ERROR_FAILURE; //we are done. cannot jump lines
    }

    nsCOMPtr<nsIFrameEnumerator> frameTraversal;
    result = NS_NewFrameTraversal(getter_AddRefs(frameTraversal),
                                  presContext, traversedFrame,
                                  eLeaf,
                                  aVisual && presContext->BidiEnabled(),
                                  aScrollViewStop,
                                  PR_TRUE  // aFollowOOFs
                                  );
    if (NS_FAILED(result))
      return result;

    if (aDirection == eDirNext)
      frameTraversal->Next();
    else
      frameTraversal->Prev();

    traversedFrame = frameTraversal->CurrentItem();
    if (!traversedFrame)
      return NS_ERROR_FAILURE;
    traversedFrame->IsSelectable(&selectable, nsnull);
  } // while (!selectable)

  *aOutOffset = (aDirection == eDirNext) ? 0 : -1;

#ifdef IBMBIDI
  if (aVisual) {
    PRUint8 newLevel = NS_GET_EMBEDDING_LEVEL(traversedFrame);
    PRUint8 newBaseLevel = NS_GET_BASE_LEVEL(traversedFrame);
    if ((newLevel & 1) != (newBaseLevel & 1)) // The new frame is reverse-direction, go to the other end
      *aOutOffset = -1 - *aOutOffset;
  }
#endif
  *aOutFrame = traversedFrame;
  return NS_OK;
}

nsIView* nsIFrame::GetClosestView(nsPoint* aOffset) const
{
  nsPoint offset(0,0);
  for (const nsIFrame *f = this; f; f = f->GetParent()) {
    if (f->HasView()) {
      if (aOffset)
        *aOffset = offset;
      return f->GetView();
    }
    offset += f->GetPosition();
  }

  NS_NOTREACHED("No view on any parent?  How did that happen?");
  return nsnull;
}


/* virtual */ void
nsFrame::ChildIsDirty(nsIFrame* aChild)
{
  NS_NOTREACHED("should never be called on a frame that doesn't inherit from "
                "nsContainerFrame");
}


#ifdef ACCESSIBILITY
already_AddRefed<nsAccessible>
nsFrame::CreateAccessible()
{
  return nsnull;
}
#endif

NS_DECLARE_FRAME_PROPERTY(OverflowAreasProperty,
                          nsIFrame::DestroyOverflowAreas)

void
nsIFrame::ClearOverflowRects()
{
  if (mOverflow.mType == NS_FRAME_OVERFLOW_LARGE) {
    Properties().Delete(OverflowAreasProperty());
  }
  mOverflow.mType = NS_FRAME_OVERFLOW_NONE;
}

/** Create or retrieve the previously stored overflow area, if the frame does 
 * not overflow and no creation is required return nsnull.
 * @return pointer to the overflow area rectangle 
 */
nsOverflowAreas*
nsIFrame::GetOverflowAreasProperty()
{
  FrameProperties props = Properties();
  nsOverflowAreas *overflow =
    static_cast<nsOverflowAreas*>(props.Get(OverflowAreasProperty()));

  if (overflow) {
    return overflow; // the property already exists
  }

  // The property isn't set yet, so allocate a new rect, set the property,
  // and return the newly allocated rect
  overflow = new nsOverflowAreas;
  props.Set(OverflowAreasProperty(), overflow);
  return overflow;
}

/** Set the overflowArea rect, storing it as deltas or a separate rect
 * depending on its size in relation to the primary frame rect.
 */
void
nsIFrame::SetOverflowAreas(const nsOverflowAreas& aOverflowAreas)
{
  if (mOverflow.mType == NS_FRAME_OVERFLOW_LARGE) {
    nsOverflowAreas *overflow =
      static_cast<nsOverflowAreas*>(Properties().Get(OverflowAreasProperty()));
    *overflow = aOverflowAreas;

    // Don't bother with converting to the deltas form if we already
    // have a property.
    return;
  }

  const nsRect& vis = aOverflowAreas.VisualOverflow();
  PRUint32 l = -vis.x, // left edge: positive delta is leftwards
           t = -vis.y, // top: positive is upwards
           r = vis.XMost() - mRect.width, // right: positive is rightwards
           b = vis.YMost() - mRect.height; // bottom: positive is downwards
  if (aOverflowAreas.ScrollableOverflow().IsEqualEdges(nsRect(nsPoint(0, 0), GetSize())) &&
      l <= NS_FRAME_OVERFLOW_DELTA_MAX &&
      t <= NS_FRAME_OVERFLOW_DELTA_MAX &&
      r <= NS_FRAME_OVERFLOW_DELTA_MAX &&
      b <= NS_FRAME_OVERFLOW_DELTA_MAX &&
      // we have to check these against zero because we *never* want to
      // set a frame as having no overflow in this function.  This is
      // because FinishAndStoreOverflow calls this function prior to
      // SetRect based on whether the overflow areas match aNewSize.
      // In the case where the overflow areas exactly match mRect but
      // do not match aNewSize, we need to store overflow in a property
      // so that our eventual SetRect/SetSize will know that it has to
      // reset our overflow areas.
      (l | t | r | b) != 0) {
    // It's a "small" overflow area so we store the deltas for each edge
    // directly in the frame, rather than allocating a separate rect.
    // If they're all zero, that's fine; we're setting things to
    // no-overflow.
    mOverflow.mVisualDeltas.mLeft   = l;
    mOverflow.mVisualDeltas.mTop    = t;
    mOverflow.mVisualDeltas.mRight  = r;
    mOverflow.mVisualDeltas.mBottom = b;
  } else {
    // it's a large overflow area that we need to store as a property
    mOverflow.mType = NS_FRAME_OVERFLOW_LARGE;
    nsOverflowAreas* overflow = GetOverflowAreasProperty();
    NS_ASSERTION(overflow, "should have created areas");
    *overflow = aOverflowAreas;
  }
}

inline PRBool
IsInlineFrame(nsIFrame *aFrame)
{
  nsIAtom *type = aFrame->GetType();
  return type == nsGkAtoms::inlineFrame ||
         type == nsGkAtoms::positionedInlineFrame;
}

void 
nsIFrame::FinishAndStoreOverflow(nsOverflowAreas& aOverflowAreas,
                                 nsSize aNewSize)
{
  nsRect bounds(nsPoint(0, 0), aNewSize);

  // This is now called FinishAndStoreOverflow() instead of 
  // StoreOverflow() because frame-generic ways of adding overflow
  // can happen here, e.g. CSS2 outline and native theme.
  NS_FOR_FRAME_OVERFLOW_TYPES(otype) {
    NS_ASSERTION(aNewSize.width == 0 || aNewSize.height == 0 ||
                 aOverflowAreas.Overflow(otype).Contains(nsRect(nsPoint(0,0), aNewSize)),
                 "Computed overflow area must contain frame bounds");
  }

  // If we clip our children, clear accumulated overflow area. The
  // children are actually clipped to the padding-box, but since the
  // overflow area should include the entire border-box, just set it to
  // the border-box here.
  const nsStyleDisplay *disp = GetStyleDisplay();
  NS_ASSERTION((disp->mOverflowY == NS_STYLE_OVERFLOW_CLIP) ==
               (disp->mOverflowX == NS_STYLE_OVERFLOW_CLIP),
               "If one overflow is clip, the other should be too");
  if (disp->mOverflowX == NS_STYLE_OVERFLOW_CLIP ||
      nsFrame::ApplyPaginatedOverflowClipping(this)) {
    // The contents are actually clipped to the padding area 
    aOverflowAreas.SetAllTo(bounds);
  }

  // Overflow area must always include the frame's top-left and bottom-right,
  // even if the frame rect is empty.
  // Pending a real fix for bug 426879, don't do this for inline frames
  // with zero width.
  if (aNewSize.width != 0 || !IsInlineFrame(this)) {
    NS_FOR_FRAME_OVERFLOW_TYPES(otype) {
      nsRect& o = aOverflowAreas.Overflow(otype);
      o.UnionRectEdges(o, bounds);
    }
  }

  // Note that NS_STYLE_OVERFLOW_CLIP doesn't clip the frame background,
  // so we add theme background overflow here so it's not clipped.
  if (!IsBoxWrapped() && IsThemed(disp)) {
    nsRect r(bounds);
    nsPresContext *presContext = PresContext();
    if (presContext->GetTheme()->
          GetWidgetOverflow(presContext->DeviceContext(), this,
                            disp->mAppearance, &r)) {
      nsRect& vo = aOverflowAreas.VisualOverflow();
      vo.UnionRectEdges(vo, r);
    }
  }

  // Nothing in here should affect scrollable overflow.
  PRBool hasOutlineOrEffects;
  aOverflowAreas.VisualOverflow() =
    ComputeOutlineAndEffectsRect(this, &hasOutlineOrEffects,
                                 aOverflowAreas.VisualOverflow(), aNewSize,
                                 PR_TRUE);

  // Absolute position clipping
  PRBool didHaveAbsPosClip = (GetStateBits() & NS_FRAME_HAS_CLIP) != 0;
  nsRect absPosClipRect;
  PRBool hasAbsPosClip = GetAbsPosClipRect(disp, &absPosClipRect, aNewSize);
  if (hasAbsPosClip) {
    NS_FOR_FRAME_OVERFLOW_TYPES(otype) {
      nsRect& o = aOverflowAreas.Overflow(otype);
      o.IntersectRect(o, absPosClipRect);
    }
    AddStateBits(NS_FRAME_HAS_CLIP);
  } else {
    RemoveStateBits(NS_FRAME_HAS_CLIP);
  }

  /* If we're transformed, transform the overflow rect by the current transformation. */
  PRBool hasTransform = IsTransformed();
  if (hasTransform) {
    Properties().Set(nsIFrame::PreTransformBBoxProperty(),
                     new nsRect(aOverflowAreas.VisualOverflow()));
    /* Since our size might not actually have been computed yet, we need to make sure that we use the
     * correct dimensions by overriding the stored bounding rectangle with the value the caller has
     * ensured us we'll use.
     */
    nsRect newBounds(nsPoint(0, 0), aNewSize);
    // Transform affects both overflow areas.
    if (!Preserves3DChildren()) {
      NS_FOR_FRAME_OVERFLOW_TYPES(otype) {
        nsRect& o = aOverflowAreas.Overflow(otype);
       o = nsDisplayTransform::TransformRect(o, this, nsPoint(0, 0), &newBounds);
      }
    } else {
      ComputePreserve3DChildrenOverflow(aOverflowAreas, newBounds);
    }
  }

  PRBool visualOverflowChanged =
    !GetVisualOverflowRect().IsEqualInterior(aOverflowAreas.VisualOverflow());

  if (aOverflowAreas != nsOverflowAreas(bounds, bounds)) {
    SetOverflowAreas(aOverflowAreas);
  } else {
    ClearOverflowRects();
  }

  if (visualOverflowChanged) {
    if (hasOutlineOrEffects) {
      // When there's an outline or box-shadow or SVG effects,
      // changes to those styles might require repainting of the old and new
      // overflow areas. Repainting of the old overflow area is handled in
      // nsCSSFrameConstructor::DoApplyRenderingChangeToTree in response
      // to nsChangeHint_RepaintFrame. Since the new overflow area is not
      // known at that time, we have to handle it here.
      // If the overflow area hasn't changed, then we don't have to do
      // anything here since repainting the old overflow area was enough.
      // If there is no outline or other effects now, then we don't have
      // to do anything here since removing those styles can't require
      // repainting of areas that weren't in the old overflow area.
      Invalidate(aOverflowAreas.VisualOverflow());
    } else if (hasAbsPosClip || didHaveAbsPosClip) {
      // If we are (or were) clipped by the 'clip' property, and our
      // overflow area changes, it might be because the clipping changed.
      // The nsChangeHint_RepaintFrame for the style change will only
      // repaint the old overflow area, so if the overflow area has
      // changed (in particular, if it grows), we have to repaint the
      // new area here.
      Invalidate(aOverflowAreas.VisualOverflow());
    } else if (hasTransform) {
      // When there's a transform, changes to that style might require
      // repainting of the old and new overflow areas in the widget.
      // Repainting of the frame itself will not be required if there's
      // a retained layer, so we can call InvalidateLayer here
      // which will avoid repainting ThebesLayers if possible.
      // nsCSSFrameConstructor::DoApplyRenderingChangeToTree repaints
      // the old overflow area in the widget in response to
      // nsChangeHint_UpdateTransformLayer. But since the new overflow
      // area is not known at that time, we have to handle it here.
      // If the overflow area hasn't changed, then it doesn't matter that
      // we didn't reach here since repainting the old overflow area was enough.
      // If there is no transform now, then the container layer for
      // the transform will go away and the frame contents will change
      // ThebesLayers, forcing it to be invalidated, so it doesn't matter
      // that we didn't reach here.
      InvalidateLayer(aOverflowAreas.VisualOverflow(),
                      nsDisplayItem::TYPE_TRANSFORM);
    }
  }
}

void
nsIFrame::ComputePreserve3DChildrenOverflow(nsOverflowAreas& aOverflowAreas, const nsRect& aBounds)
{
  // When we are preserving 3d we need to iterate over all children separately.
  // If the child also preserves 3d then their overflow will already been in our
  // coordinate space, otherwise we need to transform.
  nsRect childVisual;
  nsRect childScrollable;
  nsIFrame::ChildListIterator lists(this);
  for (; !lists.IsDone(); lists.Next()) {
    nsFrameList::Enumerator childFrames(lists.CurrentList());
    for (; !childFrames.AtEnd(); childFrames.Next()) {
      nsIFrame* child = childFrames.get();
      if (child->Preserves3D()) {
        childVisual = childVisual.Union(child->GetVisualOverflowRect());
        childScrollable = childScrollable.Union(child->GetScrollableOverflowRect());
      } else {
        childVisual = 
          childVisual.Union(nsDisplayTransform::TransformRect(child->GetVisualOverflowRect(), 
                            this, nsPoint(0,0), &aBounds));
        childScrollable = 
          childScrollable.Union(nsDisplayTransform::TransformRect(child->GetScrollableOverflowRect(), 
                                this, nsPoint(0,0), &aBounds));
      }
    }
  }

  aOverflowAreas.Overflow(eVisualOverflow) = childVisual;
  aOverflowAreas.Overflow(eScrollableOverflow) = childScrollable;
}

void
nsFrame::ConsiderChildOverflow(nsOverflowAreas& aOverflowAreas,
                               nsIFrame* aChildFrame)
{
  const nsStyleDisplay* disp = GetStyleDisplay();
  // check here also for hidden as table frames (table, tr and td) currently 
  // don't wrap their content into a scrollable frame if overflow is specified
  // FIXME: Why do we check this here rather than in
  // FinishAndStoreOverflow (where we check NS_STYLE_OVERFLOW_CLIP)?
  if (!disp->IsTableClip()) {
    aOverflowAreas.UnionWith(aChildFrame->GetOverflowAreas() +
                             aChildFrame->GetPosition());
  }
}

NS_IMETHODIMP 
nsFrame::GetParentStyleContextFrame(nsPresContext* aPresContext,
                                    nsIFrame**      aProviderFrame,
                                    PRBool*         aIsChild)
{
  return DoGetParentStyleContextFrame(aPresContext, aProviderFrame, aIsChild);
}


/**
 * This function takes a "special" frame and _if_ that frame is an anonymous
 * block created by an ib split it returns the block's preceding inline.  This
 * is needed because the split inline's style context is the parent of the
 * anonymous block's style context.
 *
 * If aFrame is not an anonymous block, null is returned.
 */
static nsIFrame*
GetIBSpecialSiblingForAnonymousBlock(nsIFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "Must have a non-null frame!");
  NS_ASSERTION(aFrame->GetStateBits() & NS_FRAME_IS_SPECIAL,
               "GetIBSpecialSibling should not be called on a non-special frame");

  nsIAtom* type = aFrame->GetStyleContext()->GetPseudo();
  if (type != nsCSSAnonBoxes::mozAnonymousBlock &&
      type != nsCSSAnonBoxes::mozAnonymousPositionedBlock) {
    // it's not an anonymous block
    return nsnull;
  }

  // Find the first continuation of the frame.  (Ugh.  This ends up
  // being O(N^2) when it is called O(N) times.)
  aFrame = aFrame->GetFirstContinuation();

  /*
   * Now look up the nsGkAtoms::IBSplitSpecialPrevSibling
   * property.
   */
  nsIFrame *specialSibling = static_cast<nsIFrame*>
    (aFrame->Properties().Get(nsIFrame::IBSplitSpecialPrevSibling()));
  NS_ASSERTION(specialSibling, "Broken frame tree?");
  return specialSibling;
}

/**
 * Get the parent, corrected for the mangled frame tree resulting from
 * having a block within an inline.  The result only differs from the
 * result of |GetParent| when |GetParent| returns an anonymous block
 * that was created for an element that was 'display: inline' because
 * that element contained a block.
 *
 * Also skip anonymous scrolled-content parents; inherit directly from the
 * outer scroll frame.
 */
static nsresult
GetCorrectedParent(nsPresContext* aPresContext, nsIFrame* aFrame,
                   nsIFrame** aSpecialParent)
{
  nsIFrame *parent = aFrame->GetParent();
  if (!parent) {
    *aSpecialParent = nsnull;
  } else {
    nsIAtom* pseudo = aFrame->GetStyleContext()->GetPseudo();
    // Outer tables are always anon boxes; if we're in here for an outer
    // table, that actually means its the _inner_ table that wants to
    // know its parent.  So get the pseudo of the inner in that case.
    if (pseudo == nsCSSAnonBoxes::tableOuter) {
      pseudo = aFrame->GetFirstPrincipalChild()
                         ->GetStyleContext()->GetPseudo();
    }
    *aSpecialParent = nsFrame::CorrectStyleParentFrame(parent, pseudo);
  }

  return NS_OK;
}

/* static */
nsIFrame*
nsFrame::CorrectStyleParentFrame(nsIFrame* aProspectiveParent,
                                 nsIAtom* aChildPseudo)
{
  NS_PRECONDITION(aProspectiveParent, "Must have a prospective parent");

  // Anon boxes are parented to their actual parent already, except
  // for non-elements.  Those should not be treated as an anon box.
  if (aChildPseudo && aChildPseudo != nsCSSAnonBoxes::mozNonElement &&
      nsCSSAnonBoxes::IsAnonBox(aChildPseudo)) {
    NS_ASSERTION(aChildPseudo != nsCSSAnonBoxes::mozAnonymousBlock &&
                 aChildPseudo != nsCSSAnonBoxes::mozAnonymousPositionedBlock,
                 "Should have dealt with kids that have NS_FRAME_IS_SPECIAL "
                 "elsewhere");
    return aProspectiveParent;
  }

  // Otherwise, walk up out of all anon boxes.  For placeholder frames, walk out
  // of all pseudo-elements as well.  Otherwise ReparentStyleContext could cause
  // style data to be out of sync with the frame tree.
  nsIFrame* parent = aProspectiveParent;
  do {
    if (parent->GetStateBits() & NS_FRAME_IS_SPECIAL) {
      nsIFrame* sibling = GetIBSpecialSiblingForAnonymousBlock(parent);

      if (sibling) {
        // |parent| was a block in an {ib} split; use the inline as
        // |the style parent.
        parent = sibling;
      }
    }
      
    nsIAtom* parentPseudo = parent->GetStyleContext()->GetPseudo();
    if (!parentPseudo ||
        (!nsCSSAnonBoxes::IsAnonBox(parentPseudo) &&
         // nsPlaceholderFrame pases in nsGkAtoms::placeholderFrame for
         // aChildPseudo (even though that's not a valid pseudo-type) just to
         // trigger this behavior of walking up to the nearest non-pseudo
         // ancestor.
         aChildPseudo != nsGkAtoms::placeholderFrame)) {
      return parent;
    }

    parent = parent->GetParent();
  } while (parent);

  if (aProspectiveParent->GetStyleContext()->GetPseudo() ==
      nsCSSAnonBoxes::viewportScroll) {
    // aProspectiveParent is the scrollframe for a viewport
    // and the kids are the anonymous scrollbars
    return aProspectiveParent;
  }

  // We can get here if the root element is absolutely positioned.
  // We can't test for this very accurately, but it can only happen
  // when the prospective parent is a canvas frame.
  NS_ASSERTION(aProspectiveParent->GetType() == nsGkAtoms::canvasFrame,
               "Should have found a parent before this");
  return nsnull;
}

nsresult
nsFrame::DoGetParentStyleContextFrame(nsPresContext* aPresContext,
                                      nsIFrame**      aProviderFrame,
                                      PRBool*         aIsChild)
{
  *aIsChild = PR_FALSE;
  *aProviderFrame = nsnull;
  if (mContent && !mContent->GetParent() &&
      !GetStyleContext()->GetPseudo()) {
    // we're a frame for the root.  We have no style context parent.
    return NS_OK;
  }
  
  if (!(mState & NS_FRAME_OUT_OF_FLOW)) {
    /*
     * If this frame is an anonymous block created when an inline with a block
     * inside it got split, then the parent style context is on its preceding
     * inline. We can get to it using GetIBSpecialSiblingForAnonymousBlock.
     */
    if (mState & NS_FRAME_IS_SPECIAL) {
      *aProviderFrame = GetIBSpecialSiblingForAnonymousBlock(this);

      if (*aProviderFrame) {
        return NS_OK;
      }
    }

    // If this frame is one of the blocks that split an inline, we must
    // return the "special" inline parent, i.e., the parent that this
    // frame would have if we didn't mangle the frame structure.
    return GetCorrectedParent(aPresContext, this, aProviderFrame);
  }

  // For out-of-flow frames, we must resolve underneath the
  // placeholder's parent.
  nsIFrame* oofFrame = this;
  if ((oofFrame->GetStateBits() & NS_FRAME_OUT_OF_FLOW) &&
      GetPrevInFlow()) {
    // Out of flows that are continuations do not
    // have placeholders. Use their first-in-flow's placeholder.
    oofFrame = oofFrame->GetFirstInFlow();
  }
  nsIFrame *placeholder =
    aPresContext->FrameManager()->GetPlaceholderFrameFor(oofFrame);
  if (!placeholder) {
    NS_NOTREACHED("no placeholder frame for out-of-flow frame");
    GetCorrectedParent(aPresContext, this, aProviderFrame);
    return NS_ERROR_FAILURE;
  }
  return static_cast<nsFrame*>(placeholder)->
    GetParentStyleContextFrame(aPresContext, aProviderFrame, aIsChild);
}

//-----------------------------------------------------------------------------------




void
nsFrame::GetLastLeaf(nsPresContext* aPresContext, nsIFrame **aFrame)
{
  if (!aFrame || !*aFrame)
    return;
  nsIFrame *child = *aFrame;
  //if we are a block frame then go for the last line of 'this'
  while (1){
    child = child->GetFirstPrincipalChild();
    if (!child)
      return;//nothing to do
    nsIFrame* siblingFrame;
    nsIContent* content;
    //ignore anonymous elements, e.g. mozTableAdd* mozTableRemove*
    //see bug 278197 comment #12 #13 for details
    while ((siblingFrame = child->GetNextSibling()) &&
           (content = siblingFrame->GetContent()) &&
           !content->IsRootOfNativeAnonymousSubtree())
      child = siblingFrame;
    *aFrame = child;
  }
}

void
nsFrame::GetFirstLeaf(nsPresContext* aPresContext, nsIFrame **aFrame)
{
  if (!aFrame || !*aFrame)
    return;
  nsIFrame *child = *aFrame;
  while (1){
    child = child->GetFirstPrincipalChild();
    if (!child)
      return;//nothing to do
    *aFrame = child;
  }
}

/* virtual */ const void*
nsFrame::GetStyleDataExternal(nsStyleStructID aSID) const
{
  NS_ASSERTION(mStyleContext, "unexpected null pointer");
  return mStyleContext->GetStyleData(aSID);
}

/* virtual */ PRBool
nsIFrame::IsFocusable(PRInt32 *aTabIndex, PRBool aWithMouse)
{
  PRInt32 tabIndex = -1;
  if (aTabIndex) {
    *aTabIndex = -1; // Default for early return is not focusable
  }
  PRBool isFocusable = PR_FALSE;

  if (mContent && mContent->IsElement() && AreAncestorViewsVisible()) {
    const nsStyleVisibility* vis = GetStyleVisibility();
    if (vis->mVisible != NS_STYLE_VISIBILITY_COLLAPSE &&
        vis->mVisible != NS_STYLE_VISIBILITY_HIDDEN) {
      const nsStyleUserInterface* ui = GetStyleUserInterface();
      if (ui->mUserFocus != NS_STYLE_USER_FOCUS_IGNORE &&
          ui->mUserFocus != NS_STYLE_USER_FOCUS_NONE) {
        // Pass in default tabindex of -1 for nonfocusable and 0 for focusable
        tabIndex = 0;
      }
      isFocusable = mContent->IsFocusable(&tabIndex, aWithMouse);
      if (!isFocusable && !aWithMouse &&
          GetType() == nsGkAtoms::scrollFrame &&
          mContent->IsHTML() &&
          !mContent->IsRootOfNativeAnonymousSubtree() &&
          mContent->GetParent() &&
          !mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::tabindex)) {
        // Elements with scrollable view are focusable with script & tabbable
        // Otherwise you couldn't scroll them with keyboard, which is
        // an accessibility issue (e.g. Section 508 rules)
        // However, we don't make them to be focusable with the mouse,
        // because the extra focus outlines are considered unnecessarily ugly.
        // When clicked on, the selection position within the element 
        // will be enough to make them keyboard scrollable.
        nsIScrollableFrame *scrollFrame = do_QueryFrame(this);
        if (scrollFrame &&
            scrollFrame->GetScrollbarStyles() != nsIScrollableFrame::ScrollbarStyles(NS_STYLE_OVERFLOW_HIDDEN, NS_STYLE_OVERFLOW_HIDDEN) &&
            !scrollFrame->GetScrollRange().IsEqualEdges(nsRect(0, 0, 0, 0))) {
            // Scroll bars will be used for overflow
            isFocusable = PR_TRUE;
            tabIndex = 0;
        }
      }
    }
  }

  if (aTabIndex) {
    *aTabIndex = tabIndex;
  }
  return isFocusable;
}

/**
 * @return PR_TRUE if this text frame ends with a newline character.  It
 * should return PR_FALSE if this is not a text frame.
 */
PRBool
nsIFrame::HasTerminalNewline() const
{
  return PR_FALSE;
}

/* static */
void nsFrame::FillCursorInformationFromStyle(const nsStyleUserInterface* ui,
                                             nsIFrame::Cursor& aCursor)
{
  aCursor.mCursor = ui->mCursor;
  aCursor.mHaveHotspot = PR_FALSE;
  aCursor.mHotspotX = aCursor.mHotspotY = 0.0f;

  for (nsCursorImage *item = ui->mCursorArray,
                 *item_end = ui->mCursorArray + ui->mCursorArrayLength;
       item < item_end; ++item) {
    PRUint32 status;
    nsresult rv = item->GetImage()->GetImageStatus(&status);
    if (NS_SUCCEEDED(rv) && (status & imgIRequest::STATUS_LOAD_COMPLETE)) {
      // This is the one we want
      item->GetImage()->GetImage(getter_AddRefs(aCursor.mContainer));
      aCursor.mHaveHotspot = item->mHaveHotspot;
      aCursor.mHotspotX = item->mHotspotX;
      aCursor.mHotspotY = item->mHotspotY;
      break;
    }
  }
}

NS_IMETHODIMP
nsFrame::RefreshSizeCache(nsBoxLayoutState& aState)
{
  // XXXbz this comment needs some rewriting to make sense in the
  // post-reflow-branch world.
  
  // Ok we need to compute our minimum, preferred, and maximum sizes.
  // 1) Maximum size. This is easy. Its infinite unless it is overloaded by CSS.
  // 2) Preferred size. This is a little harder. This is the size the block would be 
  //      if it were laid out on an infinite canvas. So we can get this by reflowing
  //      the block with and INTRINSIC width and height. We can also do a nice optimization
  //      for incremental reflow. If the reflow is incremental then we can pass a flag to 
  //      have the block compute the preferred width for us! Preferred height can just be
  //      the minimum height;
  // 3) Minimum size. This is a toughy. We can pass the block a flag asking for the max element
  //    size. That would give us the width. Unfortunately you can only ask for a maxElementSize
  //    during an incremental reflow. So on other reflows we will just have to use 0.
  //    The min height on the other hand is fairly easy we need to get the largest
  //    line height. This can be done with the line iterator.

  // if we do have a rendering context
  nsresult rv = NS_OK;
  nsRenderingContext* rendContext = aState.GetRenderingContext();
  if (rendContext) {
    nsPresContext* presContext = aState.PresContext();

    // If we don't have any HTML constraints and it's a resize, then nothing in the block
    // could have changed, so no refresh is necessary.
    nsBoxLayoutMetrics* metrics = BoxMetrics();
    if (!DoesNeedRecalc(metrics->mBlockPrefSize))
      return NS_OK;

    // get the old rect.
    nsRect oldRect = GetRect();

    // the rect we plan to size to.
    nsRect rect(oldRect);

    nsMargin bp(0,0,0,0);
    GetBorderAndPadding(bp);

    metrics->mBlockPrefSize.width = GetPrefWidth(rendContext) + bp.LeftRight();
    metrics->mBlockMinSize.width = GetMinWidth(rendContext) + bp.LeftRight();

    // do the nasty.
    nsHTMLReflowMetrics desiredSize;
    rv = BoxReflow(aState, presContext, desiredSize, rendContext,
                   rect.x, rect.y,
                   metrics->mBlockPrefSize.width, NS_UNCONSTRAINEDSIZE);

    nsRect newRect = GetRect();

    // make sure we draw any size change
    if (oldRect.width != newRect.width || oldRect.height != newRect.height) {
      newRect.x = 0;
      newRect.y = 0;
      Redraw(aState, &newRect);
    }

    metrics->mBlockMinSize.height = 0;
    // ok we need the max ascent of the items on the line. So to do this
    // ask the block for its line iterator. Get the max ascent.
    nsAutoLineIterator lines = GetLineIterator();
    if (lines) 
    {
      metrics->mBlockMinSize.height = 0;
      int count = 0;
      nsIFrame* firstFrame = nsnull;
      PRInt32 framesOnLine;
      nsRect lineBounds;
      PRUint32 lineFlags;

      do {
         lines->GetLine(count, &firstFrame, &framesOnLine, lineBounds, &lineFlags);

         if (lineBounds.height > metrics->mBlockMinSize.height)
           metrics->mBlockMinSize.height = lineBounds.height;

         count++;
      } while(firstFrame);
    } else {
      metrics->mBlockMinSize.height = desiredSize.height;
    }

    metrics->mBlockPrefSize.height = metrics->mBlockMinSize.height;

    if (desiredSize.ascent == nsHTMLReflowMetrics::ASK_FOR_BASELINE) {
      if (!nsLayoutUtils::GetFirstLineBaseline(this, &metrics->mBlockAscent))
        metrics->mBlockAscent = GetBaseline();
    } else {
      metrics->mBlockAscent = desiredSize.ascent;
    }

#ifdef DEBUG_adaptor
    printf("min=(%d,%d), pref=(%d,%d), ascent=%d\n", metrics->mBlockMinSize.width,
                                                     metrics->mBlockMinSize.height,
                                                     metrics->mBlockPrefSize.width,
                                                     metrics->mBlockPrefSize.height,
                                                     metrics->mBlockAscent);
#endif
  }

  return rv;
}

/* virtual */ nsILineIterator*
nsFrame::GetLineIterator()
{
  return nsnull;
}

nsSize
nsFrame::GetPrefSize(nsBoxLayoutState& aState)
{
  nsSize size(0,0);
  DISPLAY_PREF_SIZE(this, size);
  // If the size is cached, and there are no HTML constraints that we might
  // be depending on, then we just return the cached size.
  nsBoxLayoutMetrics *metrics = BoxMetrics();
  if (!DoesNeedRecalc(metrics->mPrefSize)) {
    return metrics->mPrefSize;
  }

  if (IsCollapsed(aState))
    return size;

  // get our size in CSS.
  PRBool widthSet, heightSet;
  PRBool completelyRedefined = nsIBox::AddCSSPrefSize(this, size, widthSet, heightSet);

  // Refresh our caches with new sizes.
  if (!completelyRedefined) {
    RefreshSizeCache(aState);
    nsSize blockSize = metrics->mBlockPrefSize;

    // notice we don't need to add our borders or padding
    // in. That's because the block did it for us.
    if (!widthSet)
      size.width = blockSize.width;
    if (!heightSet)
      size.height = blockSize.height;
  }

  metrics->mPrefSize = size;
  return size;
}

nsSize
nsFrame::GetMinSize(nsBoxLayoutState& aState)
{
  nsSize size(0,0);
  DISPLAY_MIN_SIZE(this, size);
  // Don't use the cache if we have HTMLReflowState constraints --- they might have changed
  nsBoxLayoutMetrics *metrics = BoxMetrics();
  if (!DoesNeedRecalc(metrics->mMinSize)) {
    size = metrics->mMinSize;
    return size;
  }

  if (IsCollapsed(aState))
    return size;

  // get our size in CSS.
  PRBool widthSet, heightSet;
  PRBool completelyRedefined =
    nsIBox::AddCSSMinSize(aState, this, size, widthSet, heightSet);

  // Refresh our caches with new sizes.
  if (!completelyRedefined) {
    RefreshSizeCache(aState);
    nsSize blockSize = metrics->mBlockMinSize;

    if (!widthSet)
      size.width = blockSize.width;
    if (!heightSet)
      size.height = blockSize.height;
  }

  metrics->mMinSize = size;
  return size;
}

nsSize
nsFrame::GetMaxSize(nsBoxLayoutState& aState)
{
  nsSize size(NS_INTRINSICSIZE, NS_INTRINSICSIZE);
  DISPLAY_MAX_SIZE(this, size);
  // Don't use the cache if we have HTMLReflowState constraints --- they might have changed
  nsBoxLayoutMetrics *metrics = BoxMetrics();
  if (!DoesNeedRecalc(metrics->mMaxSize)) {
    size = metrics->mMaxSize;
    return size;
  }

  if (IsCollapsed(aState))
    return size;

  size = nsBox::GetMaxSize(aState);
  metrics->mMaxSize = size;

  return size;
}

nscoord
nsFrame::GetFlex(nsBoxLayoutState& aState)
{
  nsBoxLayoutMetrics *metrics = BoxMetrics();
  if (!DoesNeedRecalc(metrics->mFlex))
     return metrics->mFlex;

  metrics->mFlex = nsBox::GetFlex(aState);

  return metrics->mFlex;
}

nscoord
nsFrame::GetBoxAscent(nsBoxLayoutState& aState)
{
  nsBoxLayoutMetrics *metrics = BoxMetrics();
  if (!DoesNeedRecalc(metrics->mAscent))
    return metrics->mAscent;

  if (IsCollapsed(aState)) {
    metrics->mAscent = 0;
  } else {
    // Refresh our caches with new sizes.
    RefreshSizeCache(aState);
    metrics->mAscent = metrics->mBlockAscent;
  }

  return metrics->mAscent;
}

nsresult
nsFrame::DoLayout(nsBoxLayoutState& aState)
{
  nsRect ourRect(mRect);

  nsRenderingContext* rendContext = aState.GetRenderingContext();
  nsPresContext* presContext = aState.PresContext();
  nsHTMLReflowMetrics desiredSize;
  nsresult rv = NS_OK;
 
  if (rendContext) {

    rv = BoxReflow(aState, presContext, desiredSize, rendContext,
                   ourRect.x, ourRect.y, ourRect.width, ourRect.height);

    if (IsCollapsed(aState)) {
      SetSize(nsSize(0, 0));
    } else {

      // if our child needs to be bigger. This might happend with
      // wrapping text. There is no way to predict its height until we
      // reflow it. Now that we know the height reshuffle upward.
      if (desiredSize.width > ourRect.width ||
          desiredSize.height > ourRect.height) {

#ifdef DEBUG_GROW
        DumpBox(stdout);
        printf(" GREW from (%d,%d) -> (%d,%d)\n",
               ourRect.width, ourRect.height,
               desiredSize.width, desiredSize.height);
#endif

        if (desiredSize.width > ourRect.width)
          ourRect.width = desiredSize.width;

        if (desiredSize.height > ourRect.height)
          ourRect.height = desiredSize.height;
      }

      // ensure our size is what we think is should be. Someone could have
      // reset the frame to be smaller or something dumb like that. 
      SetSize(nsSize(ourRect.width, ourRect.height));
    }
  }

  // Should we do this if IsCollapsed() is true?
  nsSize size(GetSize());
  desiredSize.width = size.width;
  desiredSize.height = size.height;
  desiredSize.UnionOverflowAreasWithDesiredBounds();
  FinishAndStoreOverflow(desiredSize.mOverflowAreas, size);

  SyncLayout(aState);

  return rv;
}

nsresult
nsFrame::BoxReflow(nsBoxLayoutState&        aState,
                   nsPresContext*           aPresContext,
                   nsHTMLReflowMetrics&     aDesiredSize,
                   nsRenderingContext*     aRenderingContext,
                   nscoord                  aX,
                   nscoord                  aY,
                   nscoord                  aWidth,
                   nscoord                  aHeight,
                   PRBool                   aMoveFrame)
{
  DO_GLOBAL_REFLOW_COUNT("nsBoxToBlockAdaptor");

#ifdef DEBUG_REFLOW
  nsAdaptorAddIndents();
  printf("Reflowing: ");
  nsFrame::ListTag(stdout, mFrame);
  printf("\n");
  gIndent2++;
#endif

  //printf("width=%d, height=%d\n", aWidth, aHeight);
  /*
  nsIBox* parent;
  GetParentBox(&parent);

 // if (parent->GetStateBits() & NS_STATE_CURRENTLY_IN_DEBUG)
  //   printf("In debug\n");
  */

  nsBoxLayoutMetrics *metrics = BoxMetrics();
  nsReflowStatus status = NS_FRAME_COMPLETE;

  PRBool redrawAfterReflow = PR_FALSE;
  PRBool redrawNow = PR_FALSE;

  PRBool needsReflow = NS_SUBTREE_DIRTY(this);

  if (redrawNow)
     Redraw(aState);

  // if we don't need a reflow then 
  // lets see if we are already that size. Yes? then don't even reflow. We are done.
  if (!needsReflow) {
      
      if (aWidth != NS_INTRINSICSIZE && aHeight != NS_INTRINSICSIZE) {
      
          // if the new calculated size has a 0 width or a 0 height
          if ((metrics->mLastSize.width == 0 || metrics->mLastSize.height == 0) && (aWidth == 0 || aHeight == 0)) {
               needsReflow = PR_FALSE;
               aDesiredSize.width = aWidth; 
               aDesiredSize.height = aHeight; 
               SetSize(nsSize(aDesiredSize.width, aDesiredSize.height));
          } else {
            aDesiredSize.width = metrics->mLastSize.width;
            aDesiredSize.height = metrics->mLastSize.height;

            // remove the margin. The rect of our child does not include it but our calculated size does.
            // don't reflow if we are already the right size
            if (metrics->mLastSize.width == aWidth && metrics->mLastSize.height == aHeight)
                  needsReflow = PR_FALSE;
            else
                  needsReflow = PR_TRUE;
   
          }
      } else {
          // if the width or height are intrinsic alway reflow because
          // we don't know what it should be.
         needsReflow = PR_TRUE;
      }
  }
                             
  // ok now reflow the child into the spacers calculated space
  if (needsReflow) {

    aDesiredSize.width = 0;
    aDesiredSize.height = 0;

    // create a reflow state to tell our child to flow at the given size.

    // Construct a bogus parent reflow state so that there's a usable
    // containing block reflow state.
    nsMargin margin(0,0,0,0);
    GetMargin(margin);

    nsSize parentSize(aWidth, aHeight);
    if (parentSize.height != NS_INTRINSICSIZE)
      parentSize.height += margin.TopBottom();
    if (parentSize.width != NS_INTRINSICSIZE)
      parentSize.width += margin.LeftRight();

    nsIFrame *parentFrame = GetParent();
    nsFrameState savedState = parentFrame->GetStateBits();
    nsHTMLReflowState parentReflowState(aPresContext, parentFrame,
                                        aRenderingContext,
                                        parentSize);
    parentFrame->RemoveStateBits(~nsFrameState(0));
    parentFrame->AddStateBits(savedState);

    // This may not do very much useful, but it's probably worth trying.
    if (parentSize.width != NS_INTRINSICSIZE)
      parentReflowState.SetComputedWidth(NS_MAX(parentSize.width, 0));
    if (parentSize.height != NS_INTRINSICSIZE)
      parentReflowState.SetComputedHeight(NS_MAX(parentSize.height, 0));
    parentReflowState.mComputedMargin.SizeTo(0, 0, 0, 0);
    // XXX use box methods
    parentFrame->GetPadding(parentReflowState.mComputedPadding);
    parentFrame->GetBorder(parentReflowState.mComputedBorderPadding);
    parentReflowState.mComputedBorderPadding +=
      parentReflowState.mComputedPadding;

    // XXX Is it OK that this reflow state has no parent reflow state?
    // (It used to have a bogus parent, skipping all the boxes).
    nsSize availSize(aWidth, NS_INTRINSICSIZE);
    nsHTMLReflowState reflowState(aPresContext, this, aRenderingContext,
                                  availSize);

    // Construct the parent chain manually since constructing it normally
    // messes up dimensions.
    reflowState.parentReflowState = &parentReflowState;
    reflowState.mCBReflowState = &parentReflowState;
    reflowState.mReflowDepth = aState.GetReflowDepth();

    // mComputedWidth and mComputedHeight are content-box, not
    // border-box
    if (aWidth != NS_INTRINSICSIZE) {
      nscoord computedWidth =
        aWidth - reflowState.mComputedBorderPadding.LeftRight();
      computedWidth = NS_MAX(computedWidth, 0);
      reflowState.SetComputedWidth(computedWidth);
    }

    // Most child frames of box frames (e.g. subdocument or scroll frames)
    // need to be constrained to the provided size and overflow as necessary.
    // The one exception are block frames, because we need to know their
    // natural height excluding any overflow area which may be caused by
    // various CSS effects such as shadow or outline.
    if (!IsFrameOfType(eBlockFrame)) {
      if (aHeight != NS_INTRINSICSIZE) {
        nscoord computedHeight =
          aHeight - reflowState.mComputedBorderPadding.TopBottom();
        computedHeight = NS_MAX(computedHeight, 0);
        reflowState.SetComputedHeight(computedHeight);
      } else {
        reflowState.SetComputedHeight(
          ComputeSize(aRenderingContext, availSize, availSize.width,
                      nsSize(reflowState.mComputedMargin.LeftRight(),
                             reflowState.mComputedMargin.TopBottom()),
                      nsSize(reflowState.mComputedBorderPadding.LeftRight() -
                               reflowState.mComputedPadding.LeftRight(),
                             reflowState.mComputedBorderPadding.TopBottom() -
                               reflowState.mComputedPadding.TopBottom()),
                      nsSize(reflowState.mComputedPadding.LeftRight(),
                               reflowState.mComputedPadding.TopBottom()),
                      PR_FALSE).height
          );
      }
    }

    // Box layout calls SetRect before Layout, whereas non-box layout
    // calls SetRect after Reflow.
    // XXX Perhaps we should be doing this by twiddling the rect back to
    // mLastSize before calling Reflow and then switching it back, but
    // However, mLastSize can also be the size passed to BoxReflow by
    // RefreshSizeCache, so that doesn't really make sense.
    if (metrics->mLastSize.width != aWidth)
      reflowState.mFlags.mHResize = PR_TRUE;
    if (metrics->mLastSize.height != aHeight)
      reflowState.mFlags.mVResize = PR_TRUE;

    #ifdef DEBUG_REFLOW
      nsAdaptorAddIndents();
      printf("Size=(%d,%d)\n",reflowState.ComputedWidth(),
             reflowState.ComputedHeight());
      nsAdaptorAddIndents();
      nsAdaptorPrintReason(reflowState);
      printf("\n");
    #endif

       // place the child and reflow
    WillReflow(aPresContext);

    Reflow(aPresContext, aDesiredSize, reflowState, status);

    NS_ASSERTION(NS_FRAME_IS_COMPLETE(status), "bad status");

    if (redrawAfterReflow) {
       nsRect r = GetRect();
       r.width = aDesiredSize.width;
       r.height = aDesiredSize.height;
       Redraw(aState, &r);
    }

    PRUint32 layoutFlags = aState.LayoutFlags();
    nsContainerFrame::FinishReflowChild(this, aPresContext, &reflowState,
                                        aDesiredSize, aX, aY, layoutFlags | NS_FRAME_NO_MOVE_FRAME);

    // Save the ascent.  (bug 103925)
    if (IsCollapsed(aState)) {
      metrics->mAscent = 0;
    } else {
      if (aDesiredSize.ascent == nsHTMLReflowMetrics::ASK_FOR_BASELINE) {
        if (!nsLayoutUtils::GetFirstLineBaseline(this, &metrics->mAscent))
          metrics->mAscent = GetBaseline();
      } else
        metrics->mAscent = aDesiredSize.ascent;
    }

  } else {
    aDesiredSize.ascent = metrics->mBlockAscent;
  }

#ifdef DEBUG_REFLOW
  if (aHeight != NS_INTRINSICSIZE && aDesiredSize.height != aHeight)
  {
          nsAdaptorAddIndents();
          printf("*****got taller!*****\n");
         
  }
  if (aWidth != NS_INTRINSICSIZE && aDesiredSize.width != aWidth)
  {
          nsAdaptorAddIndents();
          printf("*****got wider!******\n");
         
  }
#endif

  if (aWidth == NS_INTRINSICSIZE)
     aWidth = aDesiredSize.width;

  if (aHeight == NS_INTRINSICSIZE)
     aHeight = aDesiredSize.height;

  metrics->mLastSize.width = aDesiredSize.width;
  metrics->mLastSize.height = aDesiredSize.height;

#ifdef DEBUG_REFLOW
  gIndent2--;
#endif

  return NS_OK;
}

static void
DestroyBoxMetrics(void* aPropertyValue)
{
  delete static_cast<nsBoxLayoutMetrics*>(aPropertyValue);
}

NS_DECLARE_FRAME_PROPERTY(BoxMetricsProperty, DestroyBoxMetrics)

nsBoxLayoutMetrics*
nsFrame::BoxMetrics() const
{
  nsBoxLayoutMetrics* metrics =
    static_cast<nsBoxLayoutMetrics*>(Properties().Get(BoxMetricsProperty()));
  NS_ASSERTION(metrics, "A box layout method was called but InitBoxMetrics was never called");
  return metrics;
}

void
nsFrame::SetParent(nsIFrame* aParent)
{
  PRBool wasBoxWrapped = IsBoxWrapped();
  mParent = aParent;
  if (!wasBoxWrapped && IsBoxWrapped()) {
    InitBoxMetrics(PR_TRUE);
  } else if (wasBoxWrapped && !IsBoxWrapped()) {
    Properties().Delete(BoxMetricsProperty());
  }

  if (GetStateBits() & (NS_FRAME_HAS_VIEW | NS_FRAME_HAS_CHILD_WITH_VIEW)) {
    for (nsIFrame* f = aParent;
         f && !(f->GetStateBits() & NS_FRAME_HAS_CHILD_WITH_VIEW);
         f = f->GetParent()) {
      f->AddStateBits(NS_FRAME_HAS_CHILD_WITH_VIEW);
    }
  }

  if (GetStateBits() & NS_FRAME_HAS_CONTAINER_LAYER_DESCENDANT) {
    for (nsIFrame* f = aParent;
         f && !(f->GetStateBits() & NS_FRAME_HAS_CONTAINER_LAYER_DESCENDANT);
         f = nsLayoutUtils::GetCrossDocParentFrame(f)) {
      f->AddStateBits(NS_FRAME_HAS_CONTAINER_LAYER_DESCENDANT);
    }
  }
}

void
nsFrame::InitBoxMetrics(PRBool aClear)
{
  FrameProperties props = Properties();
  if (aClear) {
    props.Delete(BoxMetricsProperty());
  }

  nsBoxLayoutMetrics *metrics = new nsBoxLayoutMetrics();
  props.Set(BoxMetricsProperty(), metrics);

  nsFrame::MarkIntrinsicWidthsDirty();
  metrics->mBlockAscent = 0;
  metrics->mLastSize.SizeTo(0, 0);
}

// Box layout debugging
#ifdef DEBUG_REFLOW
PRInt32 gIndent2 = 0;

void
nsAdaptorAddIndents()
{
    for(PRInt32 i=0; i < gIndent2; i++)
    {
        printf(" ");
    }
}

void
nsAdaptorPrintReason(nsHTMLReflowState& aReflowState)
{
    char* reflowReasonString;

    switch(aReflowState.reason) 
    {
        case eReflowReason_Initial:
          reflowReasonString = "initial";
          break;

        case eReflowReason_Resize:
          reflowReasonString = "resize";
          break;
        case eReflowReason_Dirty:
          reflowReasonString = "dirty";
          break;
        case eReflowReason_StyleChange:
          reflowReasonString = "stylechange";
          break;
        case eReflowReason_Incremental: 
        {
            switch (aReflowState.reflowCommand->Type()) {
              case eReflowType_StyleChanged:
                 reflowReasonString = "incremental (StyleChanged)";
              break;
              case eReflowType_ReflowDirty:
                 reflowReasonString = "incremental (ReflowDirty)";
              break;
              default:
                 reflowReasonString = "incremental (Unknown)";
            }
        }                             
        break;
        default:
          reflowReasonString = "unknown";
          break;
    }

    printf("%s",reflowReasonString);
}

#endif
#ifdef DEBUG_LAYOUT
void
nsFrame::GetBoxName(nsAutoString& aName)
{
  GetFrameName(aName);
}
#endif

#ifdef NS_DEBUG
static void
GetTagName(nsFrame* aFrame, nsIContent* aContent, PRIntn aResultSize,
           char* aResult)
{
  if (aContent) {
    PR_snprintf(aResult, aResultSize, "%s@%p",
                nsAtomCString(aContent->Tag()).get(), aFrame);
  }
  else {
    PR_snprintf(aResult, aResultSize, "@%p", aFrame);
  }
}

void
nsFrame::Trace(const char* aMethod, PRBool aEnter)
{
  if (NS_FRAME_LOG_TEST(gLogModule, NS_FRAME_TRACE_CALLS)) {
    char tagbuf[40];
    GetTagName(this, mContent, sizeof(tagbuf), tagbuf);
    PR_LogPrint("%s: %s %s", tagbuf, aEnter ? "enter" : "exit", aMethod);
  }
}

void
nsFrame::Trace(const char* aMethod, PRBool aEnter, nsReflowStatus aStatus)
{
  if (NS_FRAME_LOG_TEST(gLogModule, NS_FRAME_TRACE_CALLS)) {
    char tagbuf[40];
    GetTagName(this, mContent, sizeof(tagbuf), tagbuf);
    PR_LogPrint("%s: %s %s, status=%scomplete%s",
                tagbuf, aEnter ? "enter" : "exit", aMethod,
                NS_FRAME_IS_NOT_COMPLETE(aStatus) ? "not" : "",
                (NS_FRAME_REFLOW_NEXTINFLOW & aStatus) ? "+reflow" : "");
  }
}

void
nsFrame::TraceMsg(const char* aFormatString, ...)
{
  if (NS_FRAME_LOG_TEST(gLogModule, NS_FRAME_TRACE_CALLS)) {
    // Format arguments into a buffer
    char argbuf[200];
    va_list ap;
    va_start(ap, aFormatString);
    PR_vsnprintf(argbuf, sizeof(argbuf), aFormatString, ap);
    va_end(ap);

    char tagbuf[40];
    GetTagName(this, mContent, sizeof(tagbuf), tagbuf);
    PR_LogPrint("%s: %s", tagbuf, argbuf);
  }
}

void
nsFrame::VerifyDirtyBitSet(const nsFrameList& aFrameList)
{
  for (nsFrameList::Enumerator e(aFrameList); !e.AtEnd(); e.Next()) {
    NS_ASSERTION(e.get()->GetStateBits() & NS_FRAME_IS_DIRTY,
                 "dirty bit not set");
  }
}

// Start Display Reflow
#ifdef DEBUG

DR_cookie::DR_cookie(nsPresContext*          aPresContext,
                     nsIFrame*                aFrame, 
                     const nsHTMLReflowState& aReflowState,
                     nsHTMLReflowMetrics&     aMetrics,
                     nsReflowStatus&          aStatus)
  :mPresContext(aPresContext), mFrame(aFrame), mReflowState(aReflowState), mMetrics(aMetrics), mStatus(aStatus)
{
  MOZ_COUNT_CTOR(DR_cookie);
  mValue = nsFrame::DisplayReflowEnter(aPresContext, mFrame, mReflowState);
}

DR_cookie::~DR_cookie()
{
  MOZ_COUNT_DTOR(DR_cookie);
  nsFrame::DisplayReflowExit(mPresContext, mFrame, mMetrics, mStatus, mValue);
}

DR_layout_cookie::DR_layout_cookie(nsIFrame* aFrame)
  : mFrame(aFrame)
{
  MOZ_COUNT_CTOR(DR_layout_cookie);
  mValue = nsFrame::DisplayLayoutEnter(mFrame);
}

DR_layout_cookie::~DR_layout_cookie()
{
  MOZ_COUNT_DTOR(DR_layout_cookie);
  nsFrame::DisplayLayoutExit(mFrame, mValue);
}

DR_intrinsic_width_cookie::DR_intrinsic_width_cookie(
                     nsIFrame*                aFrame, 
                     const char*              aType,
                     nscoord&                 aResult)
  : mFrame(aFrame)
  , mType(aType)
  , mResult(aResult)
{
  MOZ_COUNT_CTOR(DR_intrinsic_width_cookie);
  mValue = nsFrame::DisplayIntrinsicWidthEnter(mFrame, mType);
}

DR_intrinsic_width_cookie::~DR_intrinsic_width_cookie()
{
  MOZ_COUNT_DTOR(DR_intrinsic_width_cookie);
  nsFrame::DisplayIntrinsicWidthExit(mFrame, mType, mResult, mValue);
}

DR_intrinsic_size_cookie::DR_intrinsic_size_cookie(
                     nsIFrame*                aFrame, 
                     const char*              aType,
                     nsSize&                  aResult)
  : mFrame(aFrame)
  , mType(aType)
  , mResult(aResult)
{
  MOZ_COUNT_CTOR(DR_intrinsic_size_cookie);
  mValue = nsFrame::DisplayIntrinsicSizeEnter(mFrame, mType);
}

DR_intrinsic_size_cookie::~DR_intrinsic_size_cookie()
{
  MOZ_COUNT_DTOR(DR_intrinsic_size_cookie);
  nsFrame::DisplayIntrinsicSizeExit(mFrame, mType, mResult, mValue);
}

DR_init_constraints_cookie::DR_init_constraints_cookie(
                     nsIFrame*                aFrame,
                     nsHTMLReflowState*       aState,
                     nscoord                  aCBWidth,
                     nscoord                  aCBHeight,
                     const nsMargin*          aMargin,
                     const nsMargin*          aPadding)
  : mFrame(aFrame)
  , mState(aState)
{
  MOZ_COUNT_CTOR(DR_init_constraints_cookie);
  mValue = nsHTMLReflowState::DisplayInitConstraintsEnter(mFrame, mState,
                                                          aCBWidth, aCBHeight,
                                                          aMargin, aPadding);
}

DR_init_constraints_cookie::~DR_init_constraints_cookie()
{
  MOZ_COUNT_DTOR(DR_init_constraints_cookie);
  nsHTMLReflowState::DisplayInitConstraintsExit(mFrame, mState, mValue);
}

DR_init_offsets_cookie::DR_init_offsets_cookie(
                     nsIFrame*                aFrame,
                     nsCSSOffsetState*        aState,
                     nscoord                  aCBWidth,
                     const nsMargin*          aMargin,
                     const nsMargin*          aPadding)
  : mFrame(aFrame)
  , mState(aState)
{
  MOZ_COUNT_CTOR(DR_init_offsets_cookie);
  mValue = nsCSSOffsetState::DisplayInitOffsetsEnter(mFrame, mState, aCBWidth,
                                                     aMargin, aPadding);
}

DR_init_offsets_cookie::~DR_init_offsets_cookie()
{
  MOZ_COUNT_DTOR(DR_init_offsets_cookie);
  nsCSSOffsetState::DisplayInitOffsetsExit(mFrame, mState, mValue);
}

DR_init_type_cookie::DR_init_type_cookie(
                     nsIFrame*                aFrame,
                     nsHTMLReflowState*       aState)
  : mFrame(aFrame)
  , mState(aState)
{
  MOZ_COUNT_CTOR(DR_init_type_cookie);
  mValue = nsHTMLReflowState::DisplayInitFrameTypeEnter(mFrame, mState);
}

DR_init_type_cookie::~DR_init_type_cookie()
{
  MOZ_COUNT_DTOR(DR_init_type_cookie);
  nsHTMLReflowState::DisplayInitFrameTypeExit(mFrame, mState, mValue);
}

struct DR_FrameTypeInfo;
struct DR_FrameTreeNode;
struct DR_Rule;

struct DR_State
{
  DR_State();
  ~DR_State();
  void Init();
  void AddFrameTypeInfo(nsIAtom* aFrameType,
                        const char* aFrameNameAbbrev,
                        const char* aFrameName);
  DR_FrameTypeInfo* GetFrameTypeInfo(nsIAtom* aFrameType);
  DR_FrameTypeInfo* GetFrameTypeInfo(char* aFrameName);
  void InitFrameTypeTable();
  DR_FrameTreeNode* CreateTreeNode(nsIFrame*                aFrame,
                                   const nsHTMLReflowState* aReflowState);
  void FindMatchingRule(DR_FrameTreeNode& aNode);
  PRBool RuleMatches(DR_Rule&          aRule,
                     DR_FrameTreeNode& aNode);
  PRBool GetToken(FILE* aFile,
                  char* aBuf,
                  size_t aBufSize);
  DR_Rule* ParseRule(FILE* aFile);
  void ParseRulesFile();
  void AddRule(nsTArray<DR_Rule*>& aRules,
               DR_Rule&            aRule);
  PRBool IsWhiteSpace(int c);
  PRBool GetNumber(char*    aBuf, 
                 PRInt32&  aNumber);
  void PrettyUC(nscoord aSize,
                char*   aBuf);
  void PrintMargin(const char* tag, const nsMargin* aMargin);
  void DisplayFrameTypeInfo(nsIFrame* aFrame,
                            PRInt32   aIndent);
  void DeleteTreeNode(DR_FrameTreeNode& aNode);

  PRBool      mInited;
  PRBool      mActive;
  PRInt32     mCount;
  PRInt32     mAssert;
  PRInt32     mIndent;
  PRBool      mIndentUndisplayedFrames;
  PRBool      mDisplayPixelErrors;
  nsTArray<DR_Rule*>          mWildRules;
  nsTArray<DR_FrameTypeInfo>  mFrameTypeTable;
  // reflow specific state
  nsTArray<DR_FrameTreeNode*> mFrameTreeLeaves;
};

static DR_State *DR_state; // the one and only DR_State

struct DR_RulePart 
{
  DR_RulePart(nsIAtom* aFrameType) : mFrameType(aFrameType), mNext(0) {}
  void Destroy();

  nsIAtom*     mFrameType;
  DR_RulePart* mNext;
};

void DR_RulePart::Destroy()
{
  if (mNext) {
    mNext->Destroy();
  }
  delete this;
}

struct DR_Rule 
{
  DR_Rule() : mLength(0), mTarget(nsnull), mDisplay(PR_FALSE) {
    MOZ_COUNT_CTOR(DR_Rule);
  }
  ~DR_Rule() {
    if (mTarget) mTarget->Destroy();
    MOZ_COUNT_DTOR(DR_Rule);
  }
  void AddPart(nsIAtom* aFrameType);

  PRUint32      mLength;
  DR_RulePart*  mTarget;
  PRBool        mDisplay;
};

void DR_Rule::AddPart(nsIAtom* aFrameType)
{
  DR_RulePart* newPart = new DR_RulePart(aFrameType);
  newPart->mNext = mTarget;
  mTarget = newPart;
  mLength++;
}

struct DR_FrameTypeInfo
{
  DR_FrameTypeInfo(nsIAtom* aFrmeType, const char* aFrameNameAbbrev, const char* aFrameName);
  ~DR_FrameTypeInfo() { 
      PRInt32 numElements;
      numElements = mRules.Length();
      for (PRInt32 i = numElements - 1; i >= 0; i--) {
        delete mRules.ElementAt(i);
      }
   }

  nsIAtom*    mType;
  char        mNameAbbrev[16];
  char        mName[32];
  nsTArray<DR_Rule*> mRules;
private:
  DR_FrameTypeInfo& operator=(const DR_FrameTypeInfo&); // NOT USED
};

DR_FrameTypeInfo::DR_FrameTypeInfo(nsIAtom* aFrameType, 
                                   const char* aFrameNameAbbrev, 
                                   const char* aFrameName)
{
  mType = aFrameType;
  PL_strncpyz(mNameAbbrev, aFrameNameAbbrev, sizeof(mNameAbbrev));
  PL_strncpyz(mName, aFrameName, sizeof(mName));
}

struct DR_FrameTreeNode
{
  DR_FrameTreeNode(nsIFrame* aFrame, DR_FrameTreeNode* aParent) : mFrame(aFrame), mParent(aParent), mDisplay(0), mIndent(0)
  {
    MOZ_COUNT_CTOR(DR_FrameTreeNode);
  }

  ~DR_FrameTreeNode()
  {
    MOZ_COUNT_DTOR(DR_FrameTreeNode);
  }

  nsIFrame*         mFrame;
  DR_FrameTreeNode* mParent;
  PRBool            mDisplay;
  PRUint32          mIndent;
};

// DR_State implementation

DR_State::DR_State() 
: mInited(PR_FALSE), mActive(PR_FALSE), mCount(0), mAssert(-1), mIndent(0), 
  mIndentUndisplayedFrames(PR_FALSE), mDisplayPixelErrors(PR_FALSE)
{
  MOZ_COUNT_CTOR(DR_State);
}

void DR_State::Init() 
{
  char* env = PR_GetEnv("GECKO_DISPLAY_REFLOW_ASSERT");
  PRInt32 num;
  if (env) {
    if (GetNumber(env, num)) 
      mAssert = num;
    else 
      printf("GECKO_DISPLAY_REFLOW_ASSERT - invalid value = %s", env);
  }

  env = PR_GetEnv("GECKO_DISPLAY_REFLOW_INDENT_START");
  if (env) {
    if (GetNumber(env, num)) 
      mIndent = num;
    else 
      printf("GECKO_DISPLAY_REFLOW_INDENT_START - invalid value = %s", env);
  }

  env = PR_GetEnv("GECKO_DISPLAY_REFLOW_INDENT_UNDISPLAYED_FRAMES");
  if (env) {
    if (GetNumber(env, num)) 
      mIndentUndisplayedFrames = num;
    else 
      printf("GECKO_DISPLAY_REFLOW_INDENT_UNDISPLAYED_FRAMES - invalid value = %s", env);
  }

  env = PR_GetEnv("GECKO_DISPLAY_REFLOW_FLAG_PIXEL_ERRORS");
  if (env) {
    if (GetNumber(env, num)) 
      mDisplayPixelErrors = num;
    else 
      printf("GECKO_DISPLAY_REFLOW_FLAG_PIXEL_ERRORS - invalid value = %s", env);
  }

  InitFrameTypeTable();
  ParseRulesFile();
  mInited = PR_TRUE;
}

DR_State::~DR_State()
{
  MOZ_COUNT_DTOR(DR_State);
  PRInt32 numElements, i;
  numElements = mWildRules.Length();
  for (i = numElements - 1; i >= 0; i--) {
    delete mWildRules.ElementAt(i);
  }
  numElements = mFrameTreeLeaves.Length();
  for (i = numElements - 1; i >= 0; i--) {
    delete mFrameTreeLeaves.ElementAt(i);
  }
}

PRBool DR_State::GetNumber(char*     aBuf, 
                           PRInt32&  aNumber)
{
  if (sscanf(aBuf, "%d", &aNumber) > 0) 
    return PR_TRUE;
  else 
    return PR_FALSE;
}

PRBool DR_State::IsWhiteSpace(int c) {
  return (c == ' ') || (c == '\t') || (c == '\n') || (c == '\r');
}

PRBool DR_State::GetToken(FILE* aFile,
                          char* aBuf,
                          size_t aBufSize)
{
  PRBool haveToken = PR_FALSE;
  aBuf[0] = 0;
  // get the 1st non whitespace char
  int c = -1;
  for (c = getc(aFile); (c > 0) && IsWhiteSpace(c); c = getc(aFile)) {
  }

  if (c > 0) {
    haveToken = PR_TRUE;
    aBuf[0] = c;
    // get everything up to the next whitespace char
    size_t cX;
    for (cX = 1; cX + 1 < aBufSize ; cX++) {
      c = getc(aFile);
      if (c < 0) { // EOF
        ungetc(' ', aFile); 
        break;
      }
      else {
        if (IsWhiteSpace(c)) {
          break;
        }
        else {
          aBuf[cX] = c;
        }
      }
    }
    aBuf[cX] = 0;
  }
  return haveToken;
}

DR_Rule* DR_State::ParseRule(FILE* aFile)
{
  char buf[128];
  PRInt32 doDisplay;
  DR_Rule* rule = nsnull;
  while (GetToken(aFile, buf, sizeof(buf))) {
    if (GetNumber(buf, doDisplay)) {
      if (rule) { 
        rule->mDisplay = !!doDisplay;
        break;
      }
      else {
        printf("unexpected token - %s \n", buf);
      }
    }
    else {
      if (!rule) {
        rule = new DR_Rule;
      }
      if (strcmp(buf, "*") == 0) {
        rule->AddPart(nsnull);
      }
      else {
        DR_FrameTypeInfo* info = GetFrameTypeInfo(buf);
        if (info) {
          rule->AddPart(info->mType);
        }
        else {
          printf("invalid frame type - %s \n", buf);
        }
      }
    }
  }
  return rule;
}

void DR_State::AddRule(nsTArray<DR_Rule*>& aRules,
                       DR_Rule&            aRule)
{
  PRInt32 numRules = aRules.Length();
  for (PRInt32 ruleX = 0; ruleX < numRules; ruleX++) {
    DR_Rule* rule = aRules.ElementAt(ruleX);
    NS_ASSERTION(rule, "program error");
    if (aRule.mLength > rule->mLength) {
      aRules.InsertElementAt(ruleX, &aRule);
      return;
    }
  }
  aRules.AppendElement(&aRule);
}

void DR_State::ParseRulesFile()
{
  char* path = PR_GetEnv("GECKO_DISPLAY_REFLOW_RULES_FILE");
  if (path) {
    FILE* inFile = fopen(path, "r");
    if (inFile) {
      for (DR_Rule* rule = ParseRule(inFile); rule; rule = ParseRule(inFile)) {
        if (rule->mTarget) {
          nsIAtom* fType = rule->mTarget->mFrameType;
          if (fType) {
            DR_FrameTypeInfo* info = GetFrameTypeInfo(fType);
            if (info) {
              AddRule(info->mRules, *rule);
            }
          }
          else {
            AddRule(mWildRules, *rule);
          }
          mActive = PR_TRUE;
        }
      }
    }
  }
}


void DR_State::AddFrameTypeInfo(nsIAtom* aFrameType,
                                const char* aFrameNameAbbrev,
                                const char* aFrameName)
{
  mFrameTypeTable.AppendElement(DR_FrameTypeInfo(aFrameType, aFrameNameAbbrev, aFrameName));
}

DR_FrameTypeInfo* DR_State::GetFrameTypeInfo(nsIAtom* aFrameType)
{
  PRInt32 numEntries = mFrameTypeTable.Length();
  NS_ASSERTION(numEntries != 0, "empty FrameTypeTable");
  for (PRInt32 i = 0; i < numEntries; i++) {
    DR_FrameTypeInfo& info = mFrameTypeTable.ElementAt(i);
    if (info.mType == aFrameType) {
      return &info;
    }
  }
  return &mFrameTypeTable.ElementAt(numEntries - 1); // return unknown frame type
}

DR_FrameTypeInfo* DR_State::GetFrameTypeInfo(char* aFrameName)
{
  PRInt32 numEntries = mFrameTypeTable.Length();
  NS_ASSERTION(numEntries != 0, "empty FrameTypeTable");
  for (PRInt32 i = 0; i < numEntries; i++) {
    DR_FrameTypeInfo& info = mFrameTypeTable.ElementAt(i);
    if ((strcmp(aFrameName, info.mName) == 0) || (strcmp(aFrameName, info.mNameAbbrev) == 0)) {
      return &info;
    }
  }
  return &mFrameTypeTable.ElementAt(numEntries - 1); // return unknown frame type
}

void DR_State::InitFrameTypeTable()
{  
  AddFrameTypeInfo(nsGkAtoms::blockFrame,            "block",     "block");
  AddFrameTypeInfo(nsGkAtoms::brFrame,               "br",        "br");
  AddFrameTypeInfo(nsGkAtoms::bulletFrame,           "bullet",    "bullet");
  AddFrameTypeInfo(nsGkAtoms::gfxButtonControlFrame, "button",    "gfxButtonControl");
  AddFrameTypeInfo(nsGkAtoms::HTMLButtonControlFrame, "HTMLbutton",    "HTMLButtonControl");
  AddFrameTypeInfo(nsGkAtoms::HTMLCanvasFrame,       "HTMLCanvas","HTMLCanvas");
  AddFrameTypeInfo(nsGkAtoms::subDocumentFrame,      "subdoc",    "subDocument");
  AddFrameTypeInfo(nsGkAtoms::imageFrame,            "img",       "image");
  AddFrameTypeInfo(nsGkAtoms::inlineFrame,           "inline",    "inline");
  AddFrameTypeInfo(nsGkAtoms::letterFrame,           "letter",    "letter");
  AddFrameTypeInfo(nsGkAtoms::lineFrame,             "line",      "line");
  AddFrameTypeInfo(nsGkAtoms::listControlFrame,      "select",    "select");
  AddFrameTypeInfo(nsGkAtoms::objectFrame,           "obj",       "object");
  AddFrameTypeInfo(nsGkAtoms::pageFrame,             "page",      "page");
  AddFrameTypeInfo(nsGkAtoms::placeholderFrame,      "place",     "placeholder");
  AddFrameTypeInfo(nsGkAtoms::positionedInlineFrame, "posInline", "positionedInline");
  AddFrameTypeInfo(nsGkAtoms::canvasFrame,           "canvas",    "canvas");
  AddFrameTypeInfo(nsGkAtoms::rootFrame,             "root",      "root");
  AddFrameTypeInfo(nsGkAtoms::scrollFrame,           "scroll",    "scroll");
  AddFrameTypeInfo(nsGkAtoms::tableCaptionFrame,     "caption",   "tableCaption");
  AddFrameTypeInfo(nsGkAtoms::tableCellFrame,        "cell",      "tableCell");
  AddFrameTypeInfo(nsGkAtoms::bcTableCellFrame,      "bcCell",    "bcTableCell");
  AddFrameTypeInfo(nsGkAtoms::tableColFrame,         "col",       "tableCol");
  AddFrameTypeInfo(nsGkAtoms::tableColGroupFrame,    "colG",      "tableColGroup");
  AddFrameTypeInfo(nsGkAtoms::tableFrame,            "tbl",       "table");
  AddFrameTypeInfo(nsGkAtoms::tableOuterFrame,       "tblO",      "tableOuter");
  AddFrameTypeInfo(nsGkAtoms::tableRowGroupFrame,    "rowG",      "tableRowGroup");
  AddFrameTypeInfo(nsGkAtoms::tableRowFrame,         "row",       "tableRow");
  AddFrameTypeInfo(nsGkAtoms::textInputFrame,        "textCtl",   "textInput");
  AddFrameTypeInfo(nsGkAtoms::textFrame,             "text",      "text");
  AddFrameTypeInfo(nsGkAtoms::viewportFrame,         "VP",        "viewport");
#ifdef MOZ_XUL
  AddFrameTypeInfo(nsGkAtoms::XULLabelFrame,         "XULLabel",  "XULLabel");
  AddFrameTypeInfo(nsGkAtoms::boxFrame,              "Box",       "Box");
  AddFrameTypeInfo(nsGkAtoms::sliderFrame,           "Slider",    "Slider");
  AddFrameTypeInfo(nsGkAtoms::popupSetFrame,         "PopupSet",  "PopupSet");
#endif
  AddFrameTypeInfo(nsnull,                               "unknown",   "unknown");
}


void DR_State::DisplayFrameTypeInfo(nsIFrame* aFrame,
                                    PRInt32   aIndent)
{ 
  DR_FrameTypeInfo* frameTypeInfo = GetFrameTypeInfo(aFrame->GetType());
  if (frameTypeInfo) {
    for (PRInt32 i = 0; i < aIndent; i++) {
      printf(" ");
    }
    if(!strcmp(frameTypeInfo->mNameAbbrev, "unknown")) {
      if (aFrame) {
       nsAutoString  name;
       aFrame->GetFrameName(name);
       printf("%s %p ", NS_LossyConvertUTF16toASCII(name).get(), (void*)aFrame);
      }
      else {
        printf("%s %p ", frameTypeInfo->mNameAbbrev, (void*)aFrame);
      }
    }
    else {
      printf("%s %p ", frameTypeInfo->mNameAbbrev, (void*)aFrame);
    }
  }
}

PRBool DR_State::RuleMatches(DR_Rule&          aRule,
                             DR_FrameTreeNode& aNode)
{
  NS_ASSERTION(aRule.mTarget, "program error");

  DR_RulePart* rulePart;
  DR_FrameTreeNode* parentNode;
  for (rulePart = aRule.mTarget->mNext, parentNode = aNode.mParent;
       rulePart && parentNode;
       rulePart = rulePart->mNext, parentNode = parentNode->mParent) {
    if (rulePart->mFrameType) {
      if (parentNode->mFrame) {
        if (rulePart->mFrameType != parentNode->mFrame->GetType()) {
          return PR_FALSE;
        }
      }
      else NS_ASSERTION(PR_FALSE, "program error");
    }
    // else wild card match
  }
  return PR_TRUE;
}

void DR_State::FindMatchingRule(DR_FrameTreeNode& aNode)
{
  if (!aNode.mFrame) {
    NS_ASSERTION(PR_FALSE, "invalid DR_FrameTreeNode \n");
    return;
  }

  PRBool matchingRule = PR_FALSE;

  DR_FrameTypeInfo* info = GetFrameTypeInfo(aNode.mFrame->GetType());
  NS_ASSERTION(info, "program error");
  PRInt32 numRules = info->mRules.Length();
  for (PRInt32 ruleX = 0; ruleX < numRules; ruleX++) {
    DR_Rule* rule = info->mRules.ElementAt(ruleX);
    if (rule && RuleMatches(*rule, aNode)) {
      aNode.mDisplay = rule->mDisplay;
      matchingRule = PR_TRUE;
      break;
    }
  }
  if (!matchingRule) {
    PRInt32 numWildRules = mWildRules.Length();
    for (PRInt32 ruleX = 0; ruleX < numWildRules; ruleX++) {
      DR_Rule* rule = mWildRules.ElementAt(ruleX);
      if (rule && RuleMatches(*rule, aNode)) {
        aNode.mDisplay = rule->mDisplay;
        break;
      }
    }
  }
}
    
DR_FrameTreeNode* DR_State::CreateTreeNode(nsIFrame*                aFrame,
                                           const nsHTMLReflowState* aReflowState)
{
  // find the frame of the parent reflow state (usually just the parent of aFrame)
  nsIFrame* parentFrame;
  if (aReflowState) {
    const nsHTMLReflowState* parentRS = aReflowState->parentReflowState;
    parentFrame = (parentRS) ? parentRS->frame : nsnull;
  } else {
    parentFrame = aFrame->GetParent();
  }

  // find the parent tree node leaf
  DR_FrameTreeNode* parentNode = nsnull;
  
  DR_FrameTreeNode* lastLeaf = nsnull;
  if(mFrameTreeLeaves.Length())
    lastLeaf = mFrameTreeLeaves.ElementAt(mFrameTreeLeaves.Length() - 1);
  if (lastLeaf) {
    for (parentNode = lastLeaf; parentNode && (parentNode->mFrame != parentFrame); parentNode = parentNode->mParent) {
    }
  }
  DR_FrameTreeNode* newNode = new DR_FrameTreeNode(aFrame, parentNode);
  FindMatchingRule(*newNode);

  newNode->mIndent = mIndent;
  if (newNode->mDisplay || mIndentUndisplayedFrames) {
    ++mIndent;
  }

  if (lastLeaf && (lastLeaf == parentNode)) {
    mFrameTreeLeaves.RemoveElementAt(mFrameTreeLeaves.Length() - 1);
  }
  mFrameTreeLeaves.AppendElement(newNode);
  mCount++;

  return newNode;
}

void DR_State::PrettyUC(nscoord aSize,
                        char*   aBuf)
{
  if (NS_UNCONSTRAINEDSIZE == aSize) {
    strcpy(aBuf, "UC");
  }
  else {
    if ((nscoord)0xdeadbeefU == aSize)
    {
      strcpy(aBuf, "deadbeef");
    }
    else {
      sprintf(aBuf, "%d", aSize);
    }
  }
}

void DR_State::PrintMargin(const char *tag, const nsMargin* aMargin)
{
  if (aMargin) {
    char t[16], r[16], b[16], l[16];
    PrettyUC(aMargin->top, t);
    PrettyUC(aMargin->right, r);
    PrettyUC(aMargin->bottom, b);
    PrettyUC(aMargin->left, l);
    printf(" %s=%s,%s,%s,%s", tag, t, r, b, l);
  } else {
    // use %p here for consistency with other null-pointer printouts
    printf(" %s=%p", tag, (void*)aMargin);
  }
}

void DR_State::DeleteTreeNode(DR_FrameTreeNode& aNode)
{
  mFrameTreeLeaves.RemoveElement(&aNode);
  PRInt32 numLeaves = mFrameTreeLeaves.Length();
  if ((0 == numLeaves) || (aNode.mParent != mFrameTreeLeaves.ElementAt(numLeaves - 1))) {
    mFrameTreeLeaves.AppendElement(aNode.mParent);
  }

  if (aNode.mDisplay || mIndentUndisplayedFrames) {
    --mIndent;
  }
  // delete the tree node 
  delete &aNode;
}

static void
CheckPixelError(nscoord aSize,
                PRInt32 aPixelToTwips)
{
  if (NS_UNCONSTRAINEDSIZE != aSize) {
    if ((aSize % aPixelToTwips) > 0) {
      printf("VALUE %d is not a whole pixel \n", aSize);
    }
  }
}

static void DisplayReflowEnterPrint(nsPresContext*          aPresContext,
                                    nsIFrame*                aFrame,
                                    const nsHTMLReflowState& aReflowState,
                                    DR_FrameTreeNode&        aTreeNode,
                                    PRBool                   aChanged)
{
  if (aTreeNode.mDisplay) {
    DR_state->DisplayFrameTypeInfo(aFrame, aTreeNode.mIndent);

    char width[16];
    char height[16];

    DR_state->PrettyUC(aReflowState.availableWidth, width);
    DR_state->PrettyUC(aReflowState.availableHeight, height);
    printf("Reflow a=%s,%s ", width, height);

    DR_state->PrettyUC(aReflowState.ComputedWidth(), width);
    DR_state->PrettyUC(aReflowState.ComputedHeight(), height);
    printf("c=%s,%s ", width, height);

    if (aFrame->GetStateBits() & NS_FRAME_IS_DIRTY)
      printf("dirty ");

    if (aFrame->GetStateBits() & NS_FRAME_HAS_DIRTY_CHILDREN)
      printf("dirty-children ");

    if (aReflowState.mFlags.mSpecialHeightReflow)
      printf("special-height ");

    if (aReflowState.mFlags.mHResize)
      printf("h-resize ");

    if (aReflowState.mFlags.mVResize)
      printf("v-resize ");

    nsIFrame* inFlow = aFrame->GetPrevInFlow();
    if (inFlow) {
      printf("pif=%p ", (void*)inFlow);
    }
    inFlow = aFrame->GetNextInFlow();
    if (inFlow) {
      printf("nif=%p ", (void*)inFlow);
    }
    if (aChanged) 
      printf("CHANGED \n");
    else 
      printf("cnt=%d \n", DR_state->mCount);
    if (DR_state->mDisplayPixelErrors) {
      PRInt32 p2t = aPresContext->AppUnitsPerDevPixel();
      CheckPixelError(aReflowState.availableWidth, p2t);
      CheckPixelError(aReflowState.availableHeight, p2t);
      CheckPixelError(aReflowState.ComputedWidth(), p2t);
      CheckPixelError(aReflowState.ComputedHeight(), p2t);
    }
  }
}

void* nsFrame::DisplayReflowEnter(nsPresContext*          aPresContext,
                                  nsIFrame*                aFrame,
                                  const nsHTMLReflowState& aReflowState)
{
  if (!DR_state->mInited) DR_state->Init();
  if (!DR_state->mActive) return nsnull;

  NS_ASSERTION(aFrame, "invalid call");

  DR_FrameTreeNode* treeNode = DR_state->CreateTreeNode(aFrame, &aReflowState);
  if (treeNode) {
    DisplayReflowEnterPrint(aPresContext, aFrame, aReflowState, *treeNode, PR_FALSE);
  }
  return treeNode;
}

void* nsFrame::DisplayLayoutEnter(nsIFrame* aFrame)
{
  if (!DR_state->mInited) DR_state->Init();
  if (!DR_state->mActive) return nsnull;

  NS_ASSERTION(aFrame, "invalid call");

  DR_FrameTreeNode* treeNode = DR_state->CreateTreeNode(aFrame, nsnull);
  if (treeNode && treeNode->mDisplay) {
    DR_state->DisplayFrameTypeInfo(aFrame, treeNode->mIndent);
    printf("Layout\n");
  }
  return treeNode;
}

void* nsFrame::DisplayIntrinsicWidthEnter(nsIFrame* aFrame,
                                          const char* aType)
{
  if (!DR_state->mInited) DR_state->Init();
  if (!DR_state->mActive) return nsnull;

  NS_ASSERTION(aFrame, "invalid call");

  DR_FrameTreeNode* treeNode = DR_state->CreateTreeNode(aFrame, nsnull);
  if (treeNode && treeNode->mDisplay) {
    DR_state->DisplayFrameTypeInfo(aFrame, treeNode->mIndent);
    printf("Get%sWidth\n", aType);
  }
  return treeNode;
}

void* nsFrame::DisplayIntrinsicSizeEnter(nsIFrame* aFrame,
                                         const char* aType)
{
  if (!DR_state->mInited) DR_state->Init();
  if (!DR_state->mActive) return nsnull;

  NS_ASSERTION(aFrame, "invalid call");

  DR_FrameTreeNode* treeNode = DR_state->CreateTreeNode(aFrame, nsnull);
  if (treeNode && treeNode->mDisplay) {
    DR_state->DisplayFrameTypeInfo(aFrame, treeNode->mIndent);
    printf("Get%sSize\n", aType);
  }
  return treeNode;
}

void nsFrame::DisplayReflowExit(nsPresContext*      aPresContext,
                                nsIFrame*            aFrame,
                                nsHTMLReflowMetrics& aMetrics,
                                nsReflowStatus       aStatus,
                                void*                aFrameTreeNode)
{
  if (!DR_state->mActive) return;

  NS_ASSERTION(aFrame, "DisplayReflowExit - invalid call");
  if (!aFrameTreeNode) return;

  DR_FrameTreeNode* treeNode = (DR_FrameTreeNode*)aFrameTreeNode;
  if (treeNode->mDisplay) {
    DR_state->DisplayFrameTypeInfo(aFrame, treeNode->mIndent);

    char width[16];
    char height[16];
    char x[16];
    char y[16];
    DR_state->PrettyUC(aMetrics.width, width);
    DR_state->PrettyUC(aMetrics.height, height);
    printf("Reflow d=%s,%s", width, height);

    if (!NS_FRAME_IS_FULLY_COMPLETE(aStatus)) {
      printf(" status=0x%x", aStatus);
    }
    if (aFrame->HasOverflowAreas()) {
      DR_state->PrettyUC(aMetrics.VisualOverflow().x, x);
      DR_state->PrettyUC(aMetrics.VisualOverflow().y, y);
      DR_state->PrettyUC(aMetrics.VisualOverflow().width, width);
      DR_state->PrettyUC(aMetrics.VisualOverflow().height, height);
      printf(" vis-o=(%s,%s) %s x %s", x, y, width, height);

      nsRect storedOverflow = aFrame->GetVisualOverflowRect();
      DR_state->PrettyUC(storedOverflow.x, x);
      DR_state->PrettyUC(storedOverflow.y, y);
      DR_state->PrettyUC(storedOverflow.width, width);
      DR_state->PrettyUC(storedOverflow.height, height);
      printf(" vis-sto=(%s,%s) %s x %s", x, y, width, height);

      DR_state->PrettyUC(aMetrics.ScrollableOverflow().x, x);
      DR_state->PrettyUC(aMetrics.ScrollableOverflow().y, y);
      DR_state->PrettyUC(aMetrics.ScrollableOverflow().width, width);
      DR_state->PrettyUC(aMetrics.ScrollableOverflow().height, height);
      printf(" scr-o=(%s,%s) %s x %s", x, y, width, height);

      storedOverflow = aFrame->GetScrollableOverflowRect();
      DR_state->PrettyUC(storedOverflow.x, x);
      DR_state->PrettyUC(storedOverflow.y, y);
      DR_state->PrettyUC(storedOverflow.width, width);
      DR_state->PrettyUC(storedOverflow.height, height);
      printf(" scr-sto=(%s,%s) %s x %s", x, y, width, height);
    }
    printf("\n");
    if (DR_state->mDisplayPixelErrors) {
      PRInt32 p2t = aPresContext->AppUnitsPerDevPixel();
      CheckPixelError(aMetrics.width, p2t);
      CheckPixelError(aMetrics.height, p2t);
    }
  }
  DR_state->DeleteTreeNode(*treeNode);
}

void nsFrame::DisplayLayoutExit(nsIFrame*            aFrame,
                                void*                aFrameTreeNode)
{
  if (!DR_state->mActive) return;

  NS_ASSERTION(aFrame, "non-null frame required");
  if (!aFrameTreeNode) return;

  DR_FrameTreeNode* treeNode = (DR_FrameTreeNode*)aFrameTreeNode;
  if (treeNode->mDisplay) {
    DR_state->DisplayFrameTypeInfo(aFrame, treeNode->mIndent);
    nsRect rect = aFrame->GetRect();
    printf("Layout=%d,%d,%d,%d\n", rect.x, rect.y, rect.width, rect.height);
  }
  DR_state->DeleteTreeNode(*treeNode);
}

void nsFrame::DisplayIntrinsicWidthExit(nsIFrame*            aFrame,
                                        const char*          aType,
                                        nscoord              aResult,
                                        void*                aFrameTreeNode)
{
  if (!DR_state->mActive) return;

  NS_ASSERTION(aFrame, "non-null frame required");
  if (!aFrameTreeNode) return;

  DR_FrameTreeNode* treeNode = (DR_FrameTreeNode*)aFrameTreeNode;
  if (treeNode->mDisplay) {
    DR_state->DisplayFrameTypeInfo(aFrame, treeNode->mIndent);
    char width[16];
    DR_state->PrettyUC(aResult, width);
    printf("Get%sWidth=%s\n", aType, width);
  }
  DR_state->DeleteTreeNode(*treeNode);
}

void nsFrame::DisplayIntrinsicSizeExit(nsIFrame*            aFrame,
                                       const char*          aType,
                                       nsSize               aResult,
                                       void*                aFrameTreeNode)
{
  if (!DR_state->mActive) return;

  NS_ASSERTION(aFrame, "non-null frame required");
  if (!aFrameTreeNode) return;

  DR_FrameTreeNode* treeNode = (DR_FrameTreeNode*)aFrameTreeNode;
  if (treeNode->mDisplay) {
    DR_state->DisplayFrameTypeInfo(aFrame, treeNode->mIndent);

    char width[16];
    char height[16];
    DR_state->PrettyUC(aResult.width, width);
    DR_state->PrettyUC(aResult.height, height);
    printf("Get%sSize=%s,%s\n", aType, width, height);
  }
  DR_state->DeleteTreeNode(*treeNode);
}

/* static */ void
nsFrame::DisplayReflowStartup()
{
  DR_state = new DR_State();
}

/* static */ void
nsFrame::DisplayReflowShutdown()
{
  delete DR_state;
  DR_state = nsnull;
}

void DR_cookie::Change() const
{
  DR_FrameTreeNode* treeNode = (DR_FrameTreeNode*)mValue;
  if (treeNode && treeNode->mDisplay) {
    DisplayReflowEnterPrint(mPresContext, mFrame, mReflowState, *treeNode, PR_TRUE);
  }
}

/* static */ void*
nsHTMLReflowState::DisplayInitConstraintsEnter(nsIFrame* aFrame,
                                               nsHTMLReflowState* aState,
                                               nscoord aContainingBlockWidth,
                                               nscoord aContainingBlockHeight,
                                               const nsMargin* aBorder,
                                               const nsMargin* aPadding)
{
  NS_PRECONDITION(aFrame, "non-null frame required");
  NS_PRECONDITION(aState, "non-null state required");

  if (!DR_state->mInited) DR_state->Init();
  if (!DR_state->mActive) return nsnull;

  DR_FrameTreeNode* treeNode = DR_state->CreateTreeNode(aFrame, aState);
  if (treeNode && treeNode->mDisplay) {
    DR_state->DisplayFrameTypeInfo(aFrame, treeNode->mIndent);

    printf("InitConstraints parent=%p",
           (void*)aState->parentReflowState);

    char width[16];
    char height[16];

    DR_state->PrettyUC(aContainingBlockWidth, width);
    DR_state->PrettyUC(aContainingBlockHeight, height);
    printf(" cb=%s,%s", width, height);

    DR_state->PrettyUC(aState->availableWidth, width);
    DR_state->PrettyUC(aState->availableHeight, height);
    printf(" as=%s,%s", width, height);

    DR_state->PrintMargin("b", aBorder);
    DR_state->PrintMargin("p", aPadding);
    putchar('\n');
  }
  return treeNode;
}

/* static */ void
nsHTMLReflowState::DisplayInitConstraintsExit(nsIFrame* aFrame,
                                              nsHTMLReflowState* aState,
                                              void* aValue)
{
  NS_PRECONDITION(aFrame, "non-null frame required");
  NS_PRECONDITION(aState, "non-null state required");

  if (!DR_state->mActive) return;
  if (!aValue) return;

  DR_FrameTreeNode* treeNode = (DR_FrameTreeNode*)aValue;
  if (treeNode->mDisplay) {
    DR_state->DisplayFrameTypeInfo(aFrame, treeNode->mIndent);
    char cmiw[16], cw[16], cmxw[16], cmih[16], ch[16], cmxh[16];
    DR_state->PrettyUC(aState->mComputedMinWidth, cmiw);
    DR_state->PrettyUC(aState->mComputedWidth, cw);
    DR_state->PrettyUC(aState->mComputedMaxWidth, cmxw);
    DR_state->PrettyUC(aState->mComputedMinHeight, cmih);
    DR_state->PrettyUC(aState->mComputedHeight, ch);
    DR_state->PrettyUC(aState->mComputedMaxHeight, cmxh);
    printf("InitConstraints= cw=(%s <= %s <= %s) ch=(%s <= %s <= %s)",
           cmiw, cw, cmxw, cmih, ch, cmxh);
    DR_state->PrintMargin("co", &aState->mComputedOffsets);
    putchar('\n');
  }
  DR_state->DeleteTreeNode(*treeNode);
}


/* static */ void*
nsCSSOffsetState::DisplayInitOffsetsEnter(nsIFrame* aFrame,
                                          nsCSSOffsetState* aState,
                                          nscoord aContainingBlockWidth,
                                          const nsMargin* aBorder,
                                          const nsMargin* aPadding)
{
  NS_PRECONDITION(aFrame, "non-null frame required");
  NS_PRECONDITION(aState, "non-null state required");

  if (!DR_state->mInited) DR_state->Init();
  if (!DR_state->mActive) return nsnull;

  // aState is not necessarily a nsHTMLReflowState
  DR_FrameTreeNode* treeNode = DR_state->CreateTreeNode(aFrame, nsnull);
  if (treeNode && treeNode->mDisplay) {
    DR_state->DisplayFrameTypeInfo(aFrame, treeNode->mIndent);

    char width[16];
    DR_state->PrettyUC(aContainingBlockWidth, width);
    printf("InitOffsets cbw=%s", width);
    DR_state->PrintMargin("b", aBorder);
    DR_state->PrintMargin("p", aPadding);
    putchar('\n');
  }
  return treeNode;
}

/* static */ void
nsCSSOffsetState::DisplayInitOffsetsExit(nsIFrame* aFrame,
                                         nsCSSOffsetState* aState,
                                         void* aValue)
{
  NS_PRECONDITION(aFrame, "non-null frame required");
  NS_PRECONDITION(aState, "non-null state required");

  if (!DR_state->mActive) return;
  if (!aValue) return;

  DR_FrameTreeNode* treeNode = (DR_FrameTreeNode*)aValue;
  if (treeNode->mDisplay) {
    DR_state->DisplayFrameTypeInfo(aFrame, treeNode->mIndent);
    printf("InitOffsets=");
    DR_state->PrintMargin("m", &aState->mComputedMargin);
    DR_state->PrintMargin("p", &aState->mComputedPadding);
    DR_state->PrintMargin("p+b", &aState->mComputedBorderPadding);
    putchar('\n');
  }
  DR_state->DeleteTreeNode(*treeNode);
}

/* static */ void*
nsHTMLReflowState::DisplayInitFrameTypeEnter(nsIFrame* aFrame,
                                             nsHTMLReflowState* aState)
{
  NS_PRECONDITION(aFrame, "non-null frame required");
  NS_PRECONDITION(aState, "non-null state required");

  if (!DR_state->mInited) DR_state->Init();
  if (!DR_state->mActive) return nsnull;

  // we don't print anything here
  return DR_state->CreateTreeNode(aFrame, aState);
}

/* static */ void
nsHTMLReflowState::DisplayInitFrameTypeExit(nsIFrame* aFrame,
                                            nsHTMLReflowState* aState,
                                            void* aValue)
{
  NS_PRECONDITION(aFrame, "non-null frame required");
  NS_PRECONDITION(aState, "non-null state required");

  if (!DR_state->mActive) return;
  if (!aValue) return;

  DR_FrameTreeNode* treeNode = (DR_FrameTreeNode*)aValue;
  if (treeNode->mDisplay) {
    DR_state->DisplayFrameTypeInfo(aFrame, treeNode->mIndent);
    printf("InitFrameType");

    const nsStyleDisplay *disp = aState->mStyleDisplay;

    if (aFrame->GetStateBits() & NS_FRAME_OUT_OF_FLOW)
      printf(" out-of-flow");
    if (aFrame->GetPrevInFlow())
      printf(" prev-in-flow");
    if (disp->IsAbsolutelyPositioned())
      printf(" abspos");
    if (disp->IsFloating())
      printf(" float");

    // This array must exactly match the NS_STYLE_DISPLAY constants.
    const char *const displayTypes[] = {
      "none", "block", "inline", "inline-block", "list-item", "marker",
      "run-in", "compact", "table", "inline-table", "table-row-group",
      "table-column", "table-column-group", "table-header-group",
      "table-footer-group", "table-row", "table-cell", "table-caption",
      "box", "inline-box",
#ifdef MOZ_XUL
      "grid", "inline-grid", "grid-group", "grid-line", "stack",
      "inline-stack", "deck", "popup", "groupbox",
#endif
    };
    if (disp->mDisplay >= NS_ARRAY_LENGTH(displayTypes))
      printf(" display=%u", disp->mDisplay);
    else
      printf(" display=%s", displayTypes[disp->mDisplay]);

    // This array must exactly match the NS_CSS_FRAME_TYPE constants.
    const char *const cssFrameTypes[] = {
      "unknown", "inline", "block", "floating", "absolute", "internal-table"
    };
    nsCSSFrameType bareType = NS_FRAME_GET_TYPE(aState->mFrameType);
    bool repNoBlock = NS_FRAME_IS_REPLACED_NOBLOCK(aState->mFrameType);
    bool repBlock = NS_FRAME_IS_REPLACED_CONTAINS_BLOCK(aState->mFrameType);

    if (bareType >= NS_ARRAY_LENGTH(cssFrameTypes)) {
      printf(" result=type %u", bareType);
    } else {
      printf(" result=%s", cssFrameTypes[bareType]);
    }
    printf("%s%s\n", repNoBlock ? " +rep" : "", repBlock ? " +repBlk" : "");
  }
  DR_state->DeleteTreeNode(*treeNode);
}

#endif
// End Display Reflow

#endif
