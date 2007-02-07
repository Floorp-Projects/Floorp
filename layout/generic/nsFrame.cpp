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

/* base class of all rendering objects */

#include "nsCOMPtr.h"
#include "nsFrame.h"
#include "nsFrameList.h"
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

#include "nsIDOMText.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMHTMLHRElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDeviceContext.h"
#include "nsIEditorDocShell.h"
#include "nsIEventStateManager.h"
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
#include "nsITableLayout.h"    //selection neccesity
#include "nsITableCellLayout.h"//  "
#include "nsITextControlFrame.h"
#include "nsINameSpaceManager.h"
#include "nsIPercentHeightObserver.h"
#include "nsTextTransformer.h"

#ifdef IBMBIDI
#include "nsBidiPresUtils.h"
#endif

// For triple-click pref
#include "nsIServiceManager.h"
#ifndef MOZ_CAIRO_GFX
#include "nsISelectionImageService.h"
#endif
#include "imgIContainer.h"
#include "imgIRequest.h"
#include "gfxIImageFrame.h"
#include "nsILookAndFeel.h"
#include "nsLayoutCID.h"
#include "nsWidgetsCID.h"     // for NS_LOOKANDFEEL_CID
#include "nsUnicharUtils.h"
#include "nsLayoutErrors.h"
#include "nsHTMLContainerFrame.h"
#include "nsBoxLayoutState.h"
#include "nsBlockFrame.h"
#include "nsDisplayList.h"

#ifdef MOZ_CAIRO_GFX
#include "gfxContext.h"
#endif

static NS_DEFINE_CID(kSelectionImageService, NS_SELECTIONIMAGESERVICE_CID);
static NS_DEFINE_CID(kLookAndFeelCID,  NS_LOOKANDFEEL_CID);
static NS_DEFINE_CID(kWidgetCID, NS_CHILD_CID);

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

  PRPackedBool mWasCollapsed;
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


#include "nsICaret.h"
#include "nsILineIterator.h"

//non Hack prototypes
#if 0
static void RefreshContentFrames(nsPresContext* aPresContext, nsIContent * aStartContent, nsIContent * aEndContent);
#endif

#include "prenv.h"

// start nsIFrameDebug

#ifdef NS_DEBUG
static PRBool gShowFrameBorders = PR_FALSE;

void nsIFrameDebug::ShowFrameBorders(PRBool aEnable)
{
  gShowFrameBorders = aEnable;
}

PRBool nsIFrameDebug::GetShowFrameBorders()
{
  return gShowFrameBorders;
}

static PRBool gShowEventTargetFrameBorder = PR_FALSE;

void nsIFrameDebug::ShowEventTargetFrameBorder(PRBool aEnable)
{
  gShowEventTargetFrameBorder = aEnable;
}

PRBool nsIFrameDebug::GetShowEventTargetFrameBorder()
{
  return gShowEventTargetFrameBorder;
}

/**
 * Note: the log module is created during library initialization which
 * means that you cannot perform logging before then.
 */
static PRLogModuleInfo* gLogModule;

static PRLogModuleInfo* gFrameVerifyTreeLogModuleInfo;

static PRBool gFrameVerifyTreeEnable = PRBool(0x55);

PRBool
nsIFrameDebug::GetVerifyTreeEnable()
{
  if (gFrameVerifyTreeEnable == PRBool(0x55)) {
    if (nsnull == gFrameVerifyTreeLogModuleInfo) {
      gFrameVerifyTreeLogModuleInfo = PR_NewLogModule("frameverifytree");
      gFrameVerifyTreeEnable = 0 != gFrameVerifyTreeLogModuleInfo->level;
      printf("Note: frameverifytree is %sabled\n",
             gFrameVerifyTreeEnable ? "en" : "dis");
    }
  }
  return gFrameVerifyTreeEnable;
}

void
nsIFrameDebug::SetVerifyTreeEnable(PRBool aEnabled)
{
  gFrameVerifyTreeEnable = aEnabled;
}

static PRLogModuleInfo* gStyleVerifyTreeLogModuleInfo;

static PRBool gStyleVerifyTreeEnable = PRBool(0x55);

PRBool
nsIFrameDebug::GetVerifyStyleTreeEnable()
{
  if (gStyleVerifyTreeEnable == PRBool(0x55)) {
    if (nsnull == gStyleVerifyTreeLogModuleInfo) {
      gStyleVerifyTreeLogModuleInfo = PR_NewLogModule("styleverifytree");
      gStyleVerifyTreeEnable = 0 != gStyleVerifyTreeLogModuleInfo->level;
      printf("Note: styleverifytree is %sabled\n",
             gStyleVerifyTreeEnable ? "en" : "dis");
    }
  }
  return gStyleVerifyTreeEnable;
}

void
nsIFrameDebug::SetVerifyStyleTreeEnable(PRBool aEnabled)
{
  gStyleVerifyTreeEnable = aEnabled;
}

PRLogModuleInfo*
nsIFrameDebug::GetLogModuleInfo()
{
  if (nsnull == gLogModule) {
    gLogModule = PR_NewLogModule("frame");
  }
  return gLogModule;
}

void
nsIFrameDebug::RootFrameList(nsPresContext* aPresContext, FILE* out, PRInt32 aIndent)
{
  if((nsnull == aPresContext) || (nsnull == out))
    return;

  nsIPresShell *shell = aPresContext->GetPresShell();
  if (nsnull != shell) {
    nsIFrame* frame = shell->FrameManager()->GetRootFrame();
    if(nsnull != frame) {
      nsIFrameDebug* debugFrame;
      nsresult rv;
      rv = frame->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**)&debugFrame);
      if(NS_SUCCEEDED(rv))
        debugFrame->List(out, aIndent);
    }
  }
}
#endif
// end nsIFrameDebug

#ifndef MOZ_CAIRO_GFX
// frame image selection drawing service implementation
class SelectionImageService : public nsISelectionImageService
{
public:
  SelectionImageService();
  virtual ~SelectionImageService();
  NS_DECL_ISUPPORTS
  NS_DECL_NSISELECTIONIMAGESERVICE
private:
  nsresult CreateImage(nscolor aImageColor, imgIContainer *aContainer);
  nsCOMPtr<imgIContainer> mContainer;
  nsCOMPtr<imgIContainer> mDisabledContainer;
};

NS_IMPL_ISUPPORTS1(SelectionImageService, nsISelectionImageService)

SelectionImageService::SelectionImageService()
{
}

SelectionImageService::~SelectionImageService()
{
}

NS_IMETHODIMP
SelectionImageService::GetImage(PRInt16 aSelectionValue, imgIContainer **aContainer)
{
  *aContainer = nsnull;

  nsCOMPtr<imgIContainer>* container = &mContainer;
  nsILookAndFeel::nsColorID colorID;
  if (aSelectionValue == nsISelectionController::SELECTION_ON) {
    colorID = nsILookAndFeel::eColor_TextSelectBackground;
  } else if (aSelectionValue == nsISelectionController::SELECTION_ATTENTION) {
    colorID = nsILookAndFeel::eColor_TextSelectBackgroundAttention;
  } else {
    container = &mDisabledContainer;
    colorID = nsILookAndFeel::eColor_TextSelectBackgroundDisabled;
  }

  if (!*container) {
    nsresult result;
    *container = do_CreateInstance("@mozilla.org/image/container;1", &result);
    if (NS_FAILED(result))
      return result;

    nscolor color = NS_RGB(255, 255, 255);
    nsCOMPtr<nsILookAndFeel> look = do_GetService(kLookAndFeelCID);
    if (look)
      look->GetColor(colorID, color);
    CreateImage(color, *container);
  }

  *aContainer = *container; 
  NS_ADDREF(*aContainer);
  return NS_OK;
}

NS_IMETHODIMP
SelectionImageService::Reset()
{
  mContainer = 0;
  mDisabledContainer = 0;
  return NS_OK;
}

#define SEL_IMAGE_WIDTH 32
#define SEL_IMAGE_HEIGHT 32
#define SEL_ALPHA_AMOUNT 128

nsresult
SelectionImageService::CreateImage(nscolor aImageColor, imgIContainer *aContainer)
{
  if (aContainer)
  {
    nsresult result = aContainer->Init(SEL_IMAGE_WIDTH,SEL_IMAGE_HEIGHT,nsnull);
    if (NS_SUCCEEDED(result))
    {
      nsCOMPtr<gfxIImageFrame> image = do_CreateInstance("@mozilla.org/gfx/image/frame;2",&result);
      if (NS_SUCCEEDED(result) && image)
      {
        image->Init(0, 0, SEL_IMAGE_WIDTH, SEL_IMAGE_HEIGHT, gfxIFormats::RGB_A8, 24);
        aContainer->AppendFrame(image);

        PRUint32 bpr, abpr;
        image->GetImageBytesPerRow(&bpr);
        image->GetAlphaBytesPerRow(&abpr);

        //it's better to temporarily go after heap than put big data on stack
        unsigned char *row_data = (unsigned char *)malloc(bpr);
        if (!row_data)
          return NS_ERROR_OUT_OF_MEMORY;
        unsigned char *alpha = (unsigned char *)malloc(abpr);
        if (!alpha)
        {
          free (row_data);
          return NS_ERROR_OUT_OF_MEMORY;
        }
        unsigned char *data = row_data;

        PRInt16 i;
        for (i = 0; i < SEL_IMAGE_WIDTH; i++)
        {
#if defined(XP_WIN) || defined(XP_OS2)
          *data++ = NS_GET_B(aImageColor);
          *data++ = NS_GET_G(aImageColor);
          *data++ = NS_GET_R(aImageColor);
#else
#if defined(XP_MAC) || defined(XP_MACOSX)
          *data++ = 0;
#endif
          *data++ = NS_GET_R(aImageColor);
          *data++ = NS_GET_G(aImageColor);
          *data++ = NS_GET_B(aImageColor);
#endif
        }

        memset((void *)alpha, SEL_ALPHA_AMOUNT, abpr);

        for (i = 0; i < SEL_IMAGE_HEIGHT; i++)
        {
          image->SetAlphaData(alpha, abpr, i*abpr);
          image->SetImageData(row_data,  bpr, i*bpr);
        }
        free(row_data);
        free(alpha);
        return NS_OK;
      }
    } 
  }
  return NS_ERROR_FAILURE;
}


nsresult NS_NewSelectionImageService(nsISelectionImageService** aResult)
{
  *aResult = new SelectionImageService;
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}
#endif /* MOZ_CAIRO_GFX */

//end selection service

// a handy utility to set font
void SetFontFromStyle(nsIRenderingContext* aRC, nsStyleContext* aSC) 
{
  const nsStyleFont* font = aSC->GetStyleFont();
  const nsStyleVisibility* visibility = aSC->GetStyleVisibility();

  aRC->SetFont(font->mFont, visibility->mLangGroup);
}

void
nsWeakFrame::Init(nsIFrame* aFrame)
{
  Clear(mFrame ? mFrame->GetPresContext()->GetPresShell() : nsnull);
  mFrame = aFrame;
  if (mFrame) {
    nsIPresShell* shell = mFrame->GetPresContext()->GetPresShell();
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

// Overloaded new operator. Initializes the memory to 0 and relies on an arena
// (which comes from the presShell) to perform the allocation.
void* 
nsFrame::operator new(size_t sz, nsIPresShell* aPresShell) CPP_THROW_NEW
{
  // Check the recycle list first.
  void* result = aPresShell->AllocateFrame(sz);
  
  if (result) {
    memset(result, 0, sz);
  }

  return result;
}

// Overridden to prevent the global delete from being called, since the memory
// came out of an nsIArena instead of the global delete operator's heap.
void 
nsFrame::operator delete(void* aPtr, size_t sz)
{
  // Don't let the memory be freed, since it will be recycled
  // instead. Don't call the global operator delete.

  // Stash the size of the object in the first four bytes of the
  // freed up memory.  The Destroy method can then use this information
  // to recycle the object.
  size_t* szPtr = (size_t*)aPtr;
  *szPtr = sz;
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

/////////////////////////////////////////////////////////////////////////////
// nsISupports

nsresult nsFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(aInstancePtr, "null out param");

#ifdef DEBUG
  if (aIID.Equals(NS_GET_IID(nsIFrameDebug))) {
    *aInstancePtr = NS_STATIC_CAST(void*,NS_STATIC_CAST(nsIFrameDebug*,this));
    return NS_OK;
  }
#endif

  if (aIID.Equals(NS_GET_IID(nsIFrame)) ||
      aIID.Equals(NS_GET_IID(nsISupports))) {
    *aInstancePtr = NS_STATIC_CAST(void*,NS_STATIC_CAST(nsIFrame*,this));
    return NS_OK;
  }

  *aInstancePtr = nsnull;
  return NS_NOINTERFACE;
}

nsrefcnt nsFrame::AddRef(void)
{
  NS_WARNING("not supported for frames");
  return 1;
}

nsrefcnt nsFrame::Release(void)
{
  NS_WARNING("not supported for frames");
  return 1;
}

/////////////////////////////////////////////////////////////////////////////
// nsIFrame

NS_IMETHODIMP
nsFrame::Init(nsIContent*      aContent,
              nsIFrame*        aParent,
              nsIFrame*        aPrevInFlow)
{
  mContent = aContent;
  mParent = aParent;

  if (aContent) {
    NS_ADDREF(aContent);
    aContent->SetMayHaveFrame(PR_TRUE);
    NS_ASSERTION(mContent->MayHaveFrame(), "SetMayHaveFrame failed?");
  }

  if (aPrevInFlow) {
    // Make sure the general flags bits are the same
    nsFrameState state = aPrevInFlow->GetStateBits();

    // Make bits that are currently off (see constructor) the same:
    mState |= state & (NS_FRAME_SELECTED_CONTENT |
                       NS_FRAME_INDEPENDENT_SELECTION |
                       NS_FRAME_IS_SPECIAL);
  }
  if (mParent) {
    nsFrameState state = mParent->GetStateBits();

    // Make bits that are currently off (see constructor) the same:
    mState |= state & (NS_FRAME_INDEPENDENT_SELECTION |
                       NS_FRAME_GENERATED_CONTENT);
  }
  DidSetStyleContext();

  if (IsBoxWrapped())
    InitBoxMetrics(PR_FALSE);

  return NS_OK;
}

NS_IMETHODIMP nsFrame::SetInitialChildList(nsIAtom*        aListName,
                                           nsIFrame*       aChildList)
{
  // XXX This shouldn't be getting called at all, but currently is for backwards
  // compatility reasons...
#if 0
  NS_ERROR("not a container");
  return NS_ERROR_UNEXPECTED;
#else
  NS_ASSERTION(nsnull == aChildList, "not a container");
  return NS_OK;
#endif
}

NS_IMETHODIMP
nsFrame::AppendFrames(nsIAtom*        aListName,
                      nsIFrame*       aFrameList)
{
  NS_PRECONDITION(PR_FALSE, "not a container");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsFrame::InsertFrames(nsIAtom*        aListName,
                      nsIFrame*       aPrevFrame,
                      nsIFrame*       aFrameList)
{
  NS_PRECONDITION(PR_FALSE, "not a container");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsFrame::RemoveFrame(nsIAtom*        aListName,
                     nsIFrame*       aOldFrame)
{
  NS_PRECONDITION(PR_FALSE, "not a container");
  return NS_ERROR_UNEXPECTED;
}

void
nsFrame::Destroy()
{
  // Get the view pointer now before the frame properties disappear
  // when we call NotifyDestroyingFrame()
  nsIView* view = GetView();
  nsPresContext* presContext = GetPresContext();

  nsIPresShell *shell = presContext->GetPresShell();
  NS_ASSERTION(!(mState & NS_FRAME_OUT_OF_FLOW) ||
               !shell->FrameManager()->GetPlaceholderFrameFor(this),
               "Deleting out of flow without tearing down placeholder "
               "relationship; see comments in nsFrame.h");

  shell->NotifyDestroyingFrame(this);

  if ((mState & NS_FRAME_EXTERNAL_REFERENCE) ||
      (mState & NS_FRAME_SELECTED_CONTENT)) {
    shell->ClearFrameRefs(this);
  }

  //XXX Why is this done in nsFrame instead of some frame class
  // that actually loads images?
  presContext->StopImagesFor(this);

  if (view) {
    // Break association between view and frame
    view->SetClientData(nsnull);
    
    // Destroy the view
    view->Destroy();
  }

  // Deleting the frame doesn't really free the memory, since we're using an
  // arena for allocation, but we will get our destructors called.
  delete this;

  // Now that we're totally cleaned out, we need to add ourselves to the presshell's
  // recycler.
  size_t* sz = (size_t*)this;
  shell->FreeFrame(*sz, (void*)this);
}

NS_IMETHODIMP
nsFrame::GetOffsets(PRInt32 &aStart, PRInt32 &aEnd) const
{
  aStart = 0;
  aEnd = 0;
  return NS_OK;
}

// Subclass hook for style post processing
NS_IMETHODIMP nsFrame::DidSetStyleContext()
{
  return NS_OK;
}

/* virtual */ nsMargin
nsIFrame::GetUsedMargin() const
{
  NS_ASSERTION(!(GetStateBits() &
                 (NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN)) ||
               (GetStateBits() & NS_FRAME_IN_REFLOW),
               "cannot call on a dirty frame not currently being reflowed");

  nsMargin margin(0, 0, 0, 0);
  if (!GetStyleMargin()->GetMargin(margin)) {
    nsMargin *m = NS_STATIC_CAST(nsMargin*,
                    GetProperty(nsGkAtoms::usedMarginProperty));
    NS_ASSERTION(m, "used margin property missing (out of memory?)");
    if (m) {
      margin = *m;
    }
  }
  return margin;
}

/* virtual */ nsMargin
nsIFrame::GetUsedBorder() const
{
  NS_ASSERTION(!(GetStateBits() &
                 (NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN)) ||
               (GetStateBits() & NS_FRAME_IN_REFLOW),
               "cannot call on a dirty frame not currently being reflowed");

  // Theme methods don't use const-ness.
  nsIFrame *mutable_this = NS_CONST_CAST(nsIFrame*, this);

  const nsStyleDisplay *disp = GetStyleDisplay();
  if (mutable_this->IsThemed(disp)) {
    nsMargin result;
    nsPresContext *presContext = GetPresContext();
    presContext->GetTheme()->GetWidgetBorder(presContext->DeviceContext(),
                                             mutable_this, disp->mAppearance,
                                             &result);
    result.top = presContext->DevPixelsToAppUnits(result.top);
    result.right = presContext->DevPixelsToAppUnits(result.right);
    result.bottom = presContext->DevPixelsToAppUnits(result.bottom);
    result.left = presContext->DevPixelsToAppUnits(result.left);
    return result;
  }

  return GetStyleBorder()->GetBorder();
}

/* virtual */ nsMargin
nsIFrame::GetUsedPadding() const
{
  NS_ASSERTION(!(GetStateBits() &
                 (NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN)) ||
               (GetStateBits() & NS_FRAME_IN_REFLOW),
               "cannot call on a dirty frame not currently being reflowed");

  nsMargin padding(0, 0, 0, 0);

  // Theme methods don't use const-ness.
  nsIFrame *mutable_this = NS_CONST_CAST(nsIFrame*, this);

  const nsStyleDisplay *disp = GetStyleDisplay();
  if (mutable_this->IsThemed(disp)) {
    nsPresContext *presContext = GetPresContext();
    if (presContext->GetTheme()->GetWidgetPadding(presContext->DeviceContext(),
                                                  mutable_this,
                                                  disp->mAppearance,
                                                  &padding)) {
      padding.top = presContext->DevPixelsToAppUnits(padding.top);
      padding.right = presContext->DevPixelsToAppUnits(padding.right);
      padding.bottom = presContext->DevPixelsToAppUnits(padding.bottom);
      padding.left = presContext->DevPixelsToAppUnits(padding.left);
      return padding;
    }
  }
  if (!GetStylePadding()->GetPadding(padding)) {
    nsMargin *p = NS_STATIC_CAST(nsMargin*,
                    GetProperty(nsGkAtoms::usedPaddingProperty));
    NS_ASSERTION(p, "used padding property missing (out of memory?)");
    if (p) {
      padding = *p;
    }
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
nsIFrame::GetMarginRect() const
{
  nsMargin m(GetUsedMargin());
  ApplySkipSides(m);
  nsRect r(mRect);
  r.Inflate(m);
  return r;
}

nsRect
nsIFrame::GetPaddingRect() const
{
  nsMargin b(GetUsedBorder());
  ApplySkipSides(b);
  nsRect r(mRect);
  r.Deflate(b);
  return r;
}

nsRect
nsIFrame::GetContentRect() const
{
  nsMargin bp(GetUsedBorderAndPadding());
  ApplySkipSides(bp);
  nsRect r(mRect);
  r.Deflate(bp);
  return r;
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
  NS_ASSERTION(!(GetStateBits() & (NS_FRAME_IS_DIRTY |
                                   NS_FRAME_HAS_DIRTY_CHILDREN)),
               "frame must not be dirty");
  // Default to the bottom margin edge, per CSS2.1's definition of the
  // 'baseline' value of 'vertical-align'.
  return mRect.height + GetUsedMargin().bottom;
}

// Child frame enumeration

nsIAtom*
nsFrame::GetAdditionalChildListName(PRInt32 aIndex) const
{
  NS_PRECONDITION(aIndex >= 0, "invalid index number");
  return nsnull;
}

nsIFrame*
nsFrame::GetFirstChild(nsIAtom* aListName) const
{
  return nsnull;
}

static nsIFrame*
GetActiveSelectionFrame(nsIFrame* aFrame)
{
  nsIView* mouseGrabber;
  aFrame->GetPresContext()->GetViewManager()->GetMouseEventGrabber(mouseGrabber);
  if (mouseGrabber) {
    nsIFrame* activeFrame = nsLayoutUtils::GetFrameFor(mouseGrabber);
    if (activeFrame) {
      return activeFrame;
    }
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
  nsDisplaySelectionOverlay(nsFrame* aFrame, PRInt16 aSelectionValue)
    : nsDisplayItem(aFrame), mSelectionValue(aSelectionValue) {
    MOZ_COUNT_CTOR(nsDisplaySelectionOverlay);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplaySelectionOverlay() {
    MOZ_COUNT_DTOR(nsDisplaySelectionOverlay);
  }
#endif

  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx,
     const nsRect& aDirtyRect);
  NS_DISPLAY_DECL_NAME("SelectionOverlay")
private:
  PRInt16 mSelectionValue;
};

void nsDisplaySelectionOverlay::Paint(nsDisplayListBuilder* aBuilder,
     nsIRenderingContext* aCtx, const nsRect& aDirtyRect)
{
#ifndef MOZ_CAIRO_GFX
  nsCOMPtr<nsISelectionImageService> imageService
      = do_GetService(kSelectionImageService);
  if (!imageService)
    return;


  nsCOMPtr<imgIContainer> container;
  imageService->GetImage(mSelectionValue, getter_AddRefs(container));
  if (!container)
    return;
  
  nsRect rect(aBuilder->ToReferenceFrame(mFrame), mFrame->GetSize());
  rect.IntersectRect(rect, aDirtyRect);
  aCtx->DrawTile(container, 0, 0, &rect);
#else
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

  nsRefPtr<gfxContext> ctx = (gfxContext*)aCtx->GetNativeGraphicData(nsIRenderingContext::NATIVE_THEBES_CONTEXT);
  ctx->SetColor(c);

  nsRect rect(aBuilder->ToReferenceFrame(mFrame), mFrame->GetSize());
  rect.IntersectRect(rect, aDirtyRect);
  rect.ScaleRoundOut(1.0f / mFrame->GetPresContext()->AppUnitsPerDevPixel());
  ctx->Rectangle(gfxRect(rect.x, rect.y, rect.width, rect.height), PR_TRUE);
  ctx->Fill();
#endif
}

/********************************************************
* Refreshes each content's frame
*********************************************************/

nsresult
nsFrame::DisplaySelectionOverlay(nsDisplayListBuilder*   aBuilder,
                                 const nsDisplayListSet& aLists,
                                 PRUint16                aContentType)
{
//check frame selection state
  if ((GetStateBits() & NS_FRAME_SELECTED_CONTENT) != NS_FRAME_SELECTED_CONTENT)
    return NS_OK;
  if (!IsVisibleForPainting(aBuilder))
    return NS_OK;
    
  nsPresContext* presContext = GetPresContext();
  nsIPresShell *shell = presContext->PresShell();
  if (!shell)
    return NS_OK;

  PRInt16 displaySelection;
  nsresult rv = shell->GetSelectionFlags(&displaySelection);
  if (NS_FAILED(rv))
    return rv;
  if (!(displaySelection & aContentType))
    return NS_OK;

  nsFrameSelection* frameSelection = GetFrameSelection();
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

  return aLists.Content()->AppendNewToTop(new (aBuilder)
      nsDisplaySelectionOverlay(this, selectionValue));
}

nsresult
nsFrame::DisplayOutlineUnconditional(nsDisplayListBuilder*   aBuilder,
                                     const nsDisplayListSet& aLists)
{
  if (GetStyleOutline()->GetOutlineStyle() == NS_STYLE_BORDER_STYLE_NONE)
    return NS_OK;
    
  return aLists.Outlines()->AppendNewToTop(new (aBuilder) nsDisplayOutline(this));
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
                       const nsRect& aDirtyRect, const nsDisplayListSet& aLists)
{
  if (!IsVisibleForPainting(aBuilder))
    return NS_OK;

  return aLists.Content()->AppendNewToTop(
      new (aBuilder) nsDisplayCaret(this, aBuilder->GetCaret()));
}

PRBool
nsFrame::HasBorder()
{
  return GetStyleBorder()->GetBorder() != nsMargin(0,0,0,0);
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

  // Here we don't try to detect background propagation. Frames that might
  // receive a propagated background should just set aForceBackground to
  // PR_TRUE.
  if (aBuilder->IsForEventDelivery() || aForceBackground ||
      !GetStyleBackground()->IsTransparent() || GetStyleDisplay()->mAppearance) {
    nsresult rv = aLists.BorderBackground()->AppendNewToTop(new (aBuilder)
        nsDisplayBackground(this));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  
  if (HasBorder()) {
    nsresult rv = aLists.BorderBackground()->AppendNewToTop(new (aBuilder)
        nsDisplayBorder(this));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return DisplayOutlineUnconditional(aBuilder, aLists);
}

PRBool
nsIFrame::GetAbsPosClipRect(const nsStyleDisplay* aDisp, nsRect* aRect)
{
  NS_PRECONDITION(aRect, "Must have aRect out parameter");

  if (!aDisp->IsAbsolutelyPositioned() ||
      !(aDisp->mClipFlags & NS_STYLE_CLIP_RECT))
    return PR_FALSE;

  // Start with the 'auto' values and then factor in user specified values
  aRect->SetRect(nsPoint(0, 0), GetSize());
  if (0 == (NS_STYLE_CLIP_TOP_AUTO & aDisp->mClipFlags)) {
    aRect->y += aDisp->mClip.y;
  }
  if (0 == (NS_STYLE_CLIP_LEFT_AUTO & aDisp->mClipFlags)) {
    aRect->x += aDisp->mClip.x;
  }
  if (0 == (NS_STYLE_CLIP_RIGHT_AUTO & aDisp->mClipFlags)) {
    aRect->width = aDisp->mClip.width;
  }
  if (0 == (NS_STYLE_CLIP_BOTTOM_AUTO & aDisp->mClipFlags)) {
    aRect->height = aDisp->mClip.height;
  }
  return PR_TRUE;
}

static PRBool ApplyAbsPosClipping(nsDisplayListBuilder* aBuilder,
                                  const nsStyleDisplay* aDisp, nsIFrame* aFrame,
                                  nsRect* aRect) {
  if (!aFrame->GetAbsPosClipRect(aDisp, aRect))
    return PR_FALSE;
  
  // A moving frame should not be allowed to clip a non-moving frame.
  // Abs-pos clipping always clips frames below it in the frame tree, except
  // for when an abs-pos frame clips a fixed-pos frame. So when fixed-pos
  // elements are present we do not allow a moving abs-pos frame with
  // an out-of-flow descendant (which could be a fixed frame) child to clip
  // anything. It's OK to not clip anything, even the moving children ...
  // all that could happen is that we get unnecessarily conservative results
  // for nsLayoutUtils::ComputeRepaintRegionForCopy ... but this is a rare
  // situation.
  if (aBuilder->HasMovingFrames() &&
      aFrame->GetPresContext()->FrameManager()->GetRootFrame()->
          GetFirstChild(nsGkAtoms::fixedList) &&
      aBuilder->IsMovingFrame(aFrame))
    return PR_FALSE;

  *aRect += aBuilder->ToReferenceFrame(aFrame);

  return PR_TRUE;
}

/**
 * Returns PR_TRUE if aFrame is overflow:hidden and we should interpret
 * that as -moz-hidden-unscrollable.
 */
static PRBool ApplyOverflowHiddenClipping(nsIFrame* aFrame,
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
                                    nsIFrame* aFrame,
                                    const nsStyleDisplay* aDisp, nsRect* aRect) {
  // REVIEW: from nsContainerFrame.cpp SyncFrameViewGeometryDependentProperties,
  // except that that function used the border-edge for
  // -moz-hidden-unscrollable which I don't think is correct... Also I've
  // changed -moz-hidden-unscrollable to apply to any kind of frame.

  // Only -moz-hidden-unscrollable is handled here (and 'hidden' for table
  // frames). Other overflow clipping is applied by nsHTML/XULScrollFrame.
  if (!ApplyOverflowHiddenClipping(aFrame, aDisp)) {
    PRBool clip = aDisp->mOverflowX == NS_STYLE_OVERFLOW_CLIP;
    if (!clip)
      return PR_FALSE;
    // We allow -moz-hidden-unscrollable to apply to any kind of frame. This
    // is required by comboboxes which make their display text (an inline frame)
    // have clipping.
  }
  
  aRect->SetRect(aBuilder->ToReferenceFrame(aFrame), aFrame->GetSize());
  const nsStyleBorder* borderStyle = aFrame->GetStyleContext()->GetStyleBorder();
  aRect->Deflate(borderStyle->GetBorder());
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
                        PRBool aClipBorderBackground, PRBool aClipAll)
    : mContainer(aContainer), mRect(aRect),
      mClipBorderBackground(aClipBorderBackground), mClipAll(aClipAll) {}
  virtual PRBool WrapBorderBackground() { return mClipBorderBackground; }
  virtual nsDisplayItem* WrapList(nsDisplayListBuilder* aBuilder,
                                  nsIFrame* aFrame, nsDisplayList* aList) {
    // We are not a stacking context root. There is no valid underlying
    // frame for the whole list. These items are all in-flow descendants so
    // we can safely just clip them.
    return new (aBuilder) nsDisplayClip(nsnull, aList, mRect);
  }
  virtual nsDisplayItem* WrapItem(nsDisplayListBuilder* aBuilder,
                                  nsDisplayItem* aItem) {
    nsIFrame* f = aItem->GetUnderlyingFrame();
    if (mClipAll || nsLayoutUtils::IsProperAncestorFrame(mContainer, f, nsnull))
      return new (aBuilder) nsDisplayClip(f, aItem, mRect);
    return aItem;
  }
protected:
  nsIFrame*    mContainer;
  nsRect       mRect;
  PRPackedBool mClipBorderBackground;
  PRPackedBool mClipAll;
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
    return new (aBuilder) nsDisplayClip(nsnull, aList, mRect);
  }
  virtual nsDisplayItem* WrapItem(nsDisplayListBuilder* aBuilder,
                                  nsDisplayItem* aItem) {
    return new (aBuilder) nsDisplayClip(aItem->GetUnderlyingFrame(), aItem, mRect);
  }
protected:
  nsRect mRect;
};

nsresult
nsIFrame::OverflowClip(nsDisplayListBuilder*   aBuilder,
                       const nsDisplayListSet& aFromSet,
                       const nsDisplayListSet& aToSet,
                       const nsRect&           aClipRect,
                       PRBool                  aClipBorderBackground,
                       PRBool                  aClipAll)
{
  nsOverflowClipWrapper wrapper(this, aClipRect, aClipBorderBackground, aClipAll);
  return wrapper.WrapLists(aBuilder, this, aFromSet, aToSet);
}

nsresult
nsIFrame::Clip(nsDisplayListBuilder*   aBuilder,
               const nsDisplayListSet& aFromSet,
               const nsDisplayListSet& aToSet,
               const nsRect&           aClipRect)
{
  nsAbsPosClipWrapper wrapper(aClipRect);
  return wrapper.WrapLists(aBuilder, this, aFromSet, aToSet);
}

static nsresult
BuildDisplayListWithOverflowClip(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
    const nsRect& aDirtyRect, const nsDisplayListSet& aSet,
    const nsRect& aClipRect)
{
  nsDisplayListCollection set;
  nsresult rv = aFrame->BuildDisplayList(aBuilder, aDirtyRect, set);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aBuilder->DisplayCaret(aFrame, aDirtyRect, aSet);
  NS_ENSURE_SUCCESS(rv, rv);
  
  return aFrame->OverflowClip(aBuilder, set, aSet, aClipRect);
}

nsresult
nsIFrame::BuildDisplayListForStackingContext(nsDisplayListBuilder* aBuilder,
                                             const nsRect&         aDirtyRect,
                                             nsDisplayList*        aList) {
  if (GetStateBits() & NS_FRAME_IS_UNFLOWABLE)
    return NS_OK;

  // Replaced elements have their visibility handled here, because
  // they're visually atomic
  if (IsFrameOfType(eReplaced) && !IsVisibleForPainting(aBuilder))
    return NS_OK;

  nsRect absPosClip;
  const nsStyleDisplay* disp = GetStyleDisplay();
  PRBool applyAbsPosClipping =
      ApplyAbsPosClipping(aBuilder, disp, this, &absPosClip);
  nsRect dirtyRect = aDirtyRect;
  if (applyAbsPosClipping) {
    dirtyRect.IntersectRect(dirtyRect,
                            absPosClip - aBuilder->ToReferenceFrame(this));
  }
      
  nsDisplayListCollection set;
  nsresult rv;
  {    
    nsDisplayListBuilder::AutoIsRootSetter rootSetter(aBuilder, PR_TRUE);
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
    nsOverflowClipWrapper wrapper(this, overflowClip, PR_FALSE, PR_FALSE);
    rv = wrapper.WrapListsInPlace(aBuilder, this, set);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  // We didn't use overflowClip to restrict the dirty rect, since some of the
  // descendants may not be clipped by it. Even if we end up with unnecessary
  // display items, they'll be pruned during OptimizeVisibility.  

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
  // 6: general content
  resultList.AppendToTop(set.Content());
  // 7, 8: non-negative z-index children
  resultList.AppendToTop(set.PositionedDescendants());
  // 9: outlines, in content tree order. We need to sort by content order
  // because an element with outline that breaks and has children with outline
  // might have placed child outline items between its own outline items.
  // The element's outline items need to all come before any child outline
  // items.
  set.Outlines()->SortByContentOrder(aBuilder, GetContent());
  resultList.AppendToTop(set.Outlines());
  
  if (applyAbsPosClipping) {
    nsAbsPosClipWrapper wrapper(absPosClip);
    nsDisplayItem* item = wrapper.WrapList(aBuilder, this, &resultList);
    if (!item)
      return NS_ERROR_OUT_OF_MEMORY;
    // resultList was emptied
    resultList.AppendToTop(item);
  }
 
  if (disp->mOpacity == 1.0f) {
    aList->AppendToTop(&resultList);
  } else {
    rv = aList->AppendNewToTop(new (aBuilder) nsDisplayOpacity(this, &resultList));
  }
  
  return rv;
}

#ifdef NS_DEBUG
static void PaintDebugBorder(nsIFrame* aFrame, nsIRenderingContext* aCtx,
     const nsRect& aDirtyRect, nsPoint aPt) {
  nsRect r(aPt, aFrame->GetSize());
  if (aFrame->HasView()) {
    aCtx->SetColor(NS_RGB(0,0,255));
  } else {
    aCtx->SetColor(NS_RGB(255,0,0));
  }
  aCtx->DrawRect(r);
}
#endif

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

  if (aChild->GetStateBits() & NS_FRAME_IS_UNFLOWABLE)
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
    nsPlaceholderFrame* placeholder = NS_STATIC_CAST(nsPlaceholderFrame*, aChild);
    aChild = placeholder->GetOutOfFlowFrame();
    NS_ASSERTION(aChild, "No out of flow frame?");
    if (!aChild)
      return NS_OK;
    // update for the new child
    disp = aChild->GetStyleDisplay();
    // Make sure that any attempt to use childType below is disappointed. We
    // could call GetType again but since we don't currently need it, let's
    // avoid the virtual call.
    childType = nsnull;
    // Recheck NS_FRAME_IS_FLOWABLE
    if (aChild->GetStateBits() & NS_FRAME_IS_UNFLOWABLE)
      return NS_OK;
    nsRect* savedDirty = NS_STATIC_CAST(nsRect*,
        aChild->GetProperty(nsGkAtoms::outOfFlowDirtyRectProperty));
    if (savedDirty) {
      dirty = *savedDirty;
    } else {
      // The out-of-flow frame did not intersect the dirty area. We may still
      // need to traverse into it, since it may contain placeholders we need
      // to enter to reach other out-of-flow frames that are visible.
      dirty.Empty();
    }
    pseudoStackingContext = PR_TRUE;
  } else {
    dirty.IntersectRect(dirty, aChild->GetOverflowRect());
  }

  if (!(aChild->GetStateBits() & NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO)) {
    // No need to descend into aChild to catch placeholders for visible
    // positioned stuff. So see if we can short-circuit frame traversal here.

    // We can stop if aChild's intersection with the dirty area ended up empty.
    // If the child is a scrollframe that we want to ignore, then we need
    // to descend into it because its scrolled child may intersect the dirty
    // area even if the scrollframe itself doesn't.
    if (dirty.IsEmpty() && aChild != aBuilder->GetIgnoreScrollFrame())
      return NS_OK;

    // Note that aBuilder->GetRootMovingFrame() is non-null only if we're doing
    // ComputeRepaintRegionForCopy.
    if (aBuilder->GetRootMovingFrame() == this &&
        !GetPresContext()->GetRenderedPositionVaryingContent()) {
      // No position-varying content has been rendered in this prescontext.
      // Therefore there is no need to descend into analyzing the moving frame's
      // descendants looking for such content, because any bitblit will
      // not be copying position-varying graphics.
      return NS_OK;
    }
  }

  // XXX need to have inline-block and inline-table set pseudoStackingContext
  
  const nsStyleDisplay* ourDisp = GetStyleDisplay();
  // REVIEW: Taken from nsBoxFrame::Paint
  // Don't paint our children if the theme object is a leaf.
  if (IsThemed(ourDisp) &&
      !GetPresContext()->GetTheme()->WidgetIsContainer(ourDisp->mAppearance))
    return NS_OK;

#ifdef NS_DEBUG
  // Draw a border around the child
  // REVIEW: From nsContainerFrame::PaintChild
  if (nsIFrameDebug::GetShowFrameBorders() && !aChild->GetRect().IsEmpty()) {
    nsresult rv = aLists.Outlines()->AppendNewToTop(new (aBuilder)
        nsDisplayGeneric(aChild, PaintDebugBorder, "DebugBorder"));
    NS_ENSURE_SUCCESS(rv, rv);
  }
#endif

  PRBool isComposited = disp->mOpacity != 1.0f;
  PRBool isPositioned = disp->IsPositioned();
  if (isComposited || isPositioned || (aFlags & DISPLAY_CHILD_FORCE_STACKING_CONTEXT)) {
    // If you change this, also change IsPseudoStackingContextFromStyle()
    pseudoStackingContext = PR_TRUE;
  }
  
  nsRect overflowClip;
  PRBool applyOverflowClip =
    ApplyOverflowClipping(aBuilder, aChild, disp, &overflowClip);
  // Don't use overflowClip to restrict the dirty rect, since some of the
  // descendants may not be clipped by it. Even if we end up with unnecessary
  // display items, they'll be pruned during OptimizeVisibility. Note that
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
                                            overflowClip);
    } else {
      rv = aChild->BuildDisplayList(aBuilder, dirty, aLists);
      if (NS_SUCCEEDED(rv)) {
        rv = aBuilder->DisplayCaret(aChild, dirty, aLists);
      }
    }
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
      rv = aBuilder->DisplayCaret(aChild, dirty, aLists);
    }
  } else {
    nsRect clipRect;
    PRBool applyAbsPosClipping =
        ApplyAbsPosClipping(aBuilder, disp, aChild, &clipRect);
    // A psuedo-stacking context (e.g., a positioned element with z-index auto).
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
                                            pseudoStack, overflowClip);
    } else {
      rv = aChild->BuildDisplayList(aBuilder, clippedDirtyRect, pseudoStack);
      if (NS_SUCCEEDED(rv)) {
        rv = aBuilder->DisplayCaret(aChild, dirty, aLists);
      }
    }
    
    if (NS_SUCCEEDED(rv)) {
      if (isPositioned && applyAbsPosClipping) {
        nsAbsPosClipWrapper wrapper(clipRect);
        rv = wrapper.WrapListsInPlace(aBuilder, aChild, pseudoStack);
      }
    }
    list.AppendToTop(pseudoStack.BorderBackground());
    list.AppendToTop(pseudoStack.BlockBorderBackgrounds());
    list.AppendToTop(pseudoStack.Floats());
    list.AppendToTop(pseudoStack.Content());
    extraPositionedDescendants.AppendToTop(pseudoStack.PositionedDescendants());
    aLists.Outlines()->AppendToTop(pseudoStack.Outlines());
  }
  NS_ENSURE_SUCCESS(rv, rv);
    
  if (isPositioned || isComposited ||
      (aFlags & DISPLAY_CHILD_FORCE_STACKING_CONTEXT)) {
    // Genuine stacking contexts, and positioned pseudo-stacking-contexts,
    // go in this level.
    rv = aLists.PositionedDescendants()->AppendNewToTop(new (aBuilder)
        nsDisplayWrapList(aChild, &list));
    NS_ENSURE_SUCCESS(rv, rv);
  } else if (disp->IsFloating()) {
    rv = aLists.Floats()->AppendNewToTop(new (aBuilder)
        nsDisplayWrapList(aChild, &list));
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

nsresult
nsIFrame::CreateWidgetForView(nsIView* aView)
{
  return aView->CreateWidget(kWidgetCID);
}

/**
  *
 */
NS_IMETHODIMP  
nsFrame::GetContentForEvent(nsPresContext* aPresContext,
                            nsEvent* aEvent,
                            nsIContent** aContent)
{
  *aContent = GetContent();
  NS_IF_ADDREF(*aContent);
  return NS_OK;
}

void
nsFrame::FireDOMEvent(const nsAString& aDOMEventName, nsIContent *aContent)
{
  nsCOMPtr<nsIDOMNode> domNode = do_QueryInterface(aContent ? aContent : mContent);
  
  if (domNode) {
    nsRefPtr<nsPLDOMEvent> event = new nsPLDOMEvent(domNode, aDOMEventName);
    if (!event || NS_FAILED(event->PostDOMEvent()))
      NS_WARNING("Failed to dispatch nsPLDOMEvent");
  }
}

/**
  *
 */
NS_IMETHODIMP
nsFrame::HandleEvent(nsPresContext* aPresContext, 
                     nsGUIEvent*     aEvent,
                     nsEventStatus*  aEventStatus)
{

  if (aEvent->message == NS_MOUSE_MOVE) {
    return HandleDrag(aPresContext, aEvent, aEventStatus);
  }

  if (aEvent->eventStructType == NS_MOUSE_EVENT &&
      NS_STATIC_CAST(nsMouseEvent*, aEvent)->button == nsMouseEvent::eLeftButton) {
    if (aEvent->message == NS_MOUSE_BUTTON_DOWN) {
      HandlePress(aPresContext, aEvent, aEventStatus);
    } else if (aEvent->message == NS_MOUSE_BUTTON_UP) {
      HandleRelease(aPresContext, aEvent, aEventStatus);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::GetDataForTableSelection(nsFrameSelection *aFrameSelection,
                                  nsIPresShell *aPresShell, nsMouseEvent *aMouseEvent, 
                                  nsIContent **aParentContent, PRInt32 *aContentOffset, PRInt32 *aTarget)
{
  if (!aFrameSelection || !aPresShell || !aMouseEvent || !aParentContent || !aContentOffset || !aTarget)
    return NS_ERROR_NULL_POINTER;

  *aParentContent = nsnull;
  *aContentOffset = 0;
  *aTarget = 0;

  PRInt16 displaySelection;
  nsresult result = aPresShell->GetSelectionFlags(&displaySelection);
  if (NS_FAILED(result))
    return result;

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
#if defined(XP_MAC) || defined(XP_MACOSX)
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

  //We don't initiate row/col selection from here now,
  //  but we may in future
  //PRBool selectColumn = PR_FALSE;
  //PRBool selectRow = PR_FALSE;
  
  result = NS_OK;

  while (frame && NS_SUCCEEDED(result))
  {
    // Check for a table cell by querying to a known CellFrame interface
    nsITableCellLayout *cellElement;
    result = (frame)->QueryInterface(NS_GET_IID(nsITableCellLayout), (void **)&cellElement);
    if (NS_SUCCEEDED(result) && cellElement)
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
      nsITableLayout *tableElement;
      result = (frame)->QueryInterface(NS_GET_IID(nsITableLayout), (void **)&tableElement);
      if (NS_SUCCEEDED(result) && tableElement)
      {
        foundTable = PR_TRUE;
        //TODO: How can we select row when along left table edge
        //  or select column when along top edge?
        break;
      } else {
        frame = frame->GetParent();
        result = NS_OK;
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
  PRBool eventOK;
  aPresContext->EventStateManager()->EventStatusOK(aEvent, &eventOK);
  if (!eventOK) 
    return NS_OK;

  nsresult rv;
  nsIPresShell *shell = aPresContext->GetPresShell();
  if (!shell)
    return NS_ERROR_FAILURE;

  // if we are in Navigator and the click is in a draggable node, we don't want
  // to start selection because we don't want to interfere with a potential
  // drag of said node and steal all its glory.
  PRInt16 isEditor = 0;
  shell->GetSelectionFlags ( &isEditor );
  //weaaak. only the editor can display frame selction not just text and images
  isEditor = isEditor == nsISelectionDisplay::DISPLAY_ALL;

  nsInputEvent* keyEvent = (nsInputEvent*)aEvent;
  if (!isEditor && !keyEvent->isAlt) {
    
    for (nsIContent* content = mContent; content;
         content = content->GetParent()) {
      if ( nsContentUtils::ContentIsDraggable(content) ) {
        // coordinate stuff is the fix for bug #55921
        if ((mRect - GetPosition()).Contains(
               nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, this)))
          return NS_OK;
      }
    }
  } // if browser, not editor

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

  if (!IsMouseCaptured(aPresContext))
    CaptureMouse(aPresContext, PR_TRUE);

  // XXX This is screwy; it really should use the selection frame, not the
  // event frame
  nsFrameSelection* frameselection;
  if (useFrameSelection)
    frameselection = GetFrameSelection();
  else
    frameselection = shell->FrameSelection();

  if (frameselection->GetDisplaySelection() == nsISelectionController::SELECTION_OFF)
    return NS_OK;//nothing to do we cannot affect selection from here

  nsMouseEvent *me = (nsMouseEvent *)aEvent;

#if defined(XP_MAC) || defined(XP_MACOSX)
  if (me->isControl)
    return NS_OK;//short ciruit. hard coded for mac due to time restraints.
#endif
    
  if (me->clickCount >1 )
  {
    frameselection->SetMouseDownState(PR_TRUE);
    frameselection->SetMouseDoubleDown(PR_TRUE);
    return HandleMultiplePress(aPresContext, aEvent, aEventStatus);
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
    frameselection->SetMouseDownState(PR_TRUE);
    return frameselection->HandleTableSelection(parentContent, contentOffset, target, me);
  }

  frameselection->SetDelayedCaretData(0);

  // Check if any part of this frame is selected, and if the
  // user clicked inside the selected region. If so, we delay
  // starting a new selection since the user may be trying to
  // drag the selected region to some other app.

  SelectionDetails *details = 0;
  PRBool isSelected = ((GetStateBits() & NS_FRAME_SELECTED_CONTENT) == NS_FRAME_SELECTED_CONTENT);

  if (isSelected)
  {
    details = frameselection->LookUpSelection(offsets.content, 0,
        offsets.EndOffset(), PR_FALSE);

    //
    // If there are any details, check to see if the user clicked
    // within any selected region of the frame.
    //

    if (details)
    {
      SelectionDetails *curDetail = details;

      while (curDetail)
      {
        //
        // If the user clicked inside a selection, then just
        // return without doing anything. We will handle placing
        // the caret later on when the mouse is released. We ignore
        // the spellcheck selection.
        //
        if (curDetail->mType != nsISelectionController::SELECTION_SPELLCHECK &&
            curDetail->mStart <= offsets.StartOffset() &&
            offsets.EndOffset() <= curDetail->mEnd)
        {
          delete details;
          frameselection->SetMouseDownState(PR_FALSE);
          frameselection->SetDelayedCaretData(me);
          return NS_OK;
        }

        curDetail = curDetail->mNext;
      }

      delete details;
    }
  }

  frameselection->SetMouseDownState(PR_TRUE);

#if defined(XP_MAC) || defined(XP_MACOSX)
  PRBool control = me->isMeta;
#else
  PRBool control = me->isControl;
#endif

  rv = frameselection->HandleClick(offsets.content, offsets.StartOffset(),
                                   offsets.EndOffset(), me->isShift, control,
                                   offsets.associateWithNext);

  if (NS_FAILED(rv))
    return rv;

  if (offsets.offset != offsets.secondaryOffset)
    frameselection->MaintainSelection();

  if (isEditor && !me->isShift &&
      (offsets.EndOffset() - offsets.StartOffset()) == 1)
  {
    // A single node is selected and we aren't extending an existing
    // selection, which means the user clicked directly on an object (either
    // -moz-user-select: all or a non-text node without children).
    // Therefore, disable selection extension during mouse moves.
    // XXX This is a bit hacky; shouldn't editor be able to deal with this?
    frameselection->SetMouseDownState(PR_FALSE);
  }

  return rv;
}

/**
  * Multiple Mouse Press -- line or paragraph selection -- for the frame.
  * Wouldn't it be nice if this didn't have to be hardwired into Frame code?
 */
NS_IMETHODIMP
nsFrame::HandleMultiplePress(nsPresContext* aPresContext, 
                             nsGUIEvent*     aEvent,
                             nsEventStatus*  aEventStatus)
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
    if (nsContentUtils::GetBoolPref("browser.triple_click_selects_paragraph")) {
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
  theFrame = GetPresContext()->GetPresShell()->FrameSelection()->
    GetFrameForNodeOffset(offsets.content, offsets.offset,
                          nsFrameSelection::HINT(offsets.associateWithNext),
                          &offset);
  if (!theFrame)
    return NS_ERROR_FAILURE;

  nsFrame* frame = NS_STATIC_CAST(nsFrame*, theFrame);

  return frame->PeekBackwardAndForward(beginAmount, endAmount,
                                       offsets.offset, aPresContext,
                                       beginAmount != eSelectWord);
}

NS_IMETHODIMP
nsFrame::PeekBackwardAndForward(nsSelectionAmount aAmountBack,
                                nsSelectionAmount aAmountForward,
                                PRInt32 aStartPos,
                                nsPresContext* aPresContext,
                                PRBool aJumpLines)
{
  nsCOMPtr<nsISelectionController> selcon;
  nsresult rv = GetSelectionController(aPresContext, getter_AddRefs(selcon));
  if (NS_FAILED(rv)) return rv;

  if (!selcon)
    return NS_ERROR_NOT_INITIALIZED;

  // Use peek offset one way then the other:
  nsCOMPtr<nsIContent> startContent;
  nsCOMPtr<nsIDOMNode> startNode;
  nsCOMPtr<nsIContent> endContent;
  nsCOMPtr<nsIDOMNode> endNode;

  nsIFrame* baseFrame = this;
  PRInt32 baseOffset = aStartPos;
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

  endNode = do_QueryInterface(endpos.mResultContent, &rv);
  if (NS_FAILED(rv))
    return rv;
  startNode = do_QueryInterface(startpos.mResultContent, &rv);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsISelection> selection;
  if (NS_SUCCEEDED(selcon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                                        getter_AddRefs(selection)))){
    rv = selection->Collapse(startNode,startpos.mContentOffset);
    if (NS_FAILED(rv))
      return rv;
    rv = selection->Extend(endNode,endpos.mContentOffset);
    if (NS_FAILED(rv))
      return rv;
  }
  //no release 

  // maintain selection
  return GetFrameSelection()->MaintainSelection(aAmountBack);
}

// Figure out which view we should point capturing at, given that drag started
// in this frame.
static nsIView* GetNearestCapturingView(nsIFrame* aFrame) {
  nsIView* view = nsnull;
  while (!(view = aFrame->GetMouseCapturer()) && aFrame->GetParent()) {
    aFrame = aFrame->GetParent();
  }
  if (!view) {
    // Use the root view. The root frame always has the root view.
    view = aFrame->GetView();
  }
  NS_ASSERTION(view, "No capturing view found");
  return view;
}

nsIFrame* nsFrame::GetNearestCapturingFrame(nsIFrame* aFrame) {
  nsIFrame* captureFrame = aFrame;
  while (captureFrame && !captureFrame->GetMouseCapturer()) {
    captureFrame = captureFrame->GetParent();
  }
  return captureFrame;
}

NS_IMETHODIMP nsFrame::HandleDrag(nsPresContext* aPresContext, 
                                  nsGUIEvent*     aEvent,
                                  nsEventStatus*  aEventStatus)
{
  PRBool  selectable;
  PRUint8 selectStyle;
  IsSelectable(&selectable, &selectStyle);
  // XXX Do we really need to exclude non-selectable content here?
  // GetContentAndOffsetsFromPoint can handle it just fine, although some
  // other stuff might not like it.
  if (!selectable)
    return NS_OK;
  if (DisplaySelection(aPresContext) == nsISelectionController::SELECTION_OFF) {
    return NS_OK;
  }
  nsIPresShell *presShell = aPresContext->PresShell();

  nsCOMPtr<nsFrameSelection> frameselection = GetFrameSelection();
  PRBool mouseDown = frameselection->GetMouseDownState();
  if (!mouseDown)
    return NS_OK;

  frameselection->StopAutoScrollTimer();

  // If we have capturing view, it must be ensured that |this| doesn't 
  // get deleted during HandleDrag.
  nsWeakFrame weakFrame = GetNearestCapturingView(this) ? this : nsnull;
#ifdef NS_DEBUG
  PRBool frameAlive = weakFrame.IsAlive();
#endif

  // Check if we are dragging in a table cell
  nsCOMPtr<nsIContent> parentContent;
  PRInt32 contentOffset;
  PRInt32 target;
  nsMouseEvent *me = (nsMouseEvent *)aEvent;
  nsresult result;
  result = GetDataForTableSelection(frameselection, presShell, me,
                                    getter_AddRefs(parentContent),
                                    &contentOffset, &target);      

  if (NS_SUCCEEDED(result) && parentContent) {
    frameselection->HandleTableSelection(parentContent, contentOffset, target, me);
  } else {
    nsPoint pt = nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, this);
    frameselection->HandleDrag(this, pt);
  }

  if (weakFrame) {
    nsIView* captureView = GetNearestCapturingView(this);
    if (captureView) {
      // Get the view that aEvent->point is relative to. This is disgusting.
      nsIView* eventView = nsnull;
      nsPoint pt = nsLayoutUtils::GetEventCoordinatesForNearestView(aEvent, this,
                                                                    &eventView);
      nsPoint capturePt = pt + eventView->GetOffsetTo(captureView);
      frameselection->StartAutoScrollTimer(captureView, capturePt, 30);
    }
  }
#ifdef NS_DEBUG
  if (frameAlive && !weakFrame.IsAlive()) {
    NS_WARNING("nsFrame deleted during nsFrame::HandleDrag.");
  }
#endif
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
  nsIFrame* activeFrame = GetActiveSelectionFrame(this);

  // We can unconditionally stop capturing because
  // we should never be capturing when the mouse button is up
  CaptureMouse(aPresContext, PR_FALSE);

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
        GetDataForTableSelection(frameselection, GetPresContext()->PresShell(),
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
  if (activeFrame != this &&
      NS_STATIC_CAST(nsFrame*, activeFrame)->DisplaySelection(activeFrame->GetPresContext())
        != nsISelectionController::SELECTION_OFF) {
    nsRefPtr<nsFrameSelection> frameSelection =
      activeFrame->GetFrameSelection();
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

struct FrameContentRange {
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
  FrameTarget(nsIFrame* aFrame, PRBool aFrameEdge, PRBool aAfterFrame) :
    frame(aFrame), frameEdge(aFrameEdge), afterFrame(aAfterFrame) { }
  static FrameTarget Null() {
    return FrameTarget(nsnull, PR_FALSE, PR_FALSE);
  }
  PRBool IsNull() {
    return !frame;
  }
  nsIFrame* frame;
  PRPackedBool frameEdge;
  PRPackedBool afterFrame;
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
    nsIFrame *frame = aFrame->GetFirstChild(nsnull);
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
// blocks or blocks with no lines.
static FrameTarget GetSelectionClosestFrameForBlock(nsIFrame* aFrame,
                                                    nsPoint aPoint)
{
  nsresult rv;
  nsBlockFrame* bf; // used only for QI
  rv = aFrame->QueryInterface(kBlockFrameCID, (void**)&bf);
  if (NS_FAILED(rv))
    return FrameTarget::Null();

  // This code searches for the correct line
  nsBlockFrame::line_iterator firstLine = bf->begin_lines();
  nsBlockFrame::line_iterator end = bf->end_lines();
  if (firstLine == end)
    return FrameTarget::Null();
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
            nsContentUtils::GetIntPref("browser.drag_out_of_frame_style");

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

  nsIFrame *kid = aFrame->GetFirstChild(nsnull);

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
        xDistance = PR_MIN(abs(fromLeft), abs(fromRight));
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
          yDistance = PR_MIN(abs(fromTop), abs(fromBottom));

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
  return NS_STATIC_CAST(nsFrame*, closest.frame)->CalcContentOffsetsFromFramePoint(pt);

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
nsFrame::GetMinWidth(nsIRenderingContext *aRenderingContext)
{
#ifdef DEBUG
  nsAutoString frameName;
  GetFrameName(frameName);
  NS_WARNING(nsCAutoString(NS_ConvertUTF16toUTF8(frameName) +
    nsDependentCString(" frame didn't implement GetMinWidth")).get());
#endif
  nscoord result = 0;
  DISPLAY_MIN_WIDTH(this, result);
  return result;
}

/* virtual */ nscoord
nsFrame::GetPrefWidth(nsIRenderingContext *aRenderingContext)
{
#ifdef DEBUG
  nsAutoString frameName;
  GetFrameName(frameName);
  NS_WARNING(nsCAutoString(NS_ConvertUTF16toUTF8(frameName) +
    nsDependentCString(" frame didn't implement GetPrefWidth")).get());
#endif
  nscoord result = 0;
  DISPLAY_PREF_WIDTH(this, result);
  return result;
}

/* virtual */ void
nsFrame::AddInlineMinWidth(nsIRenderingContext *aRenderingContext,
                           nsIFrame::InlineMinWidthData *aData)
{
  NS_ASSERTION(GetParent(), "Must have a parent if we get here!");
  PRBool canBreak = !CanContinueTextRun() &&
    GetParent()->GetStyleText()->WhiteSpaceCanWrap();
  
  if (canBreak)
    aData->Break(aRenderingContext);
  aData->trailingWhitespace = 0;
  aData->skipWhitespace = PR_FALSE;
  aData->trailingTextFrame = nsnull;
  aData->currentLine += nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                            this, nsLayoutUtils::MIN_WIDTH);
  if (canBreak)
    aData->Break(aRenderingContext);
}

/* virtual */ void
nsFrame::AddInlinePrefWidth(nsIRenderingContext *aRenderingContext,
                            nsIFrame::InlinePrefWidthData *aData)
{
  aData->trailingWhitespace = 0;
  aData->skipWhitespace = PR_FALSE;
  aData->currentLine += nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                            this, nsLayoutUtils::PREF_WIDTH);
}

void
nsIFrame::InlineMinWidthData::Break(nsIRenderingContext *aRenderingContext)
{
  currentLine -= trailingWhitespace;
  prevLines = PR_MAX(prevLines, currentLine);
  currentLine = trailingWhitespace = 0;

  for (PRInt32 i = 0, i_end = floats.Count(); i != i_end; ++i) {
    nsIFrame *floatFrame = NS_STATIC_CAST(nsIFrame*, floats[i]);
    nscoord float_min =
      nsLayoutUtils::IntrinsicForContainer(aRenderingContext, floatFrame,
                                           nsLayoutUtils::MIN_WIDTH);
    if (float_min > prevLines)
      prevLines = float_min;
  }
  floats.Clear();
  trailingTextFrame = nsnull;
}

void
nsIFrame::InlinePrefWidthData::Break(nsIRenderingContext *aRenderingContext)
{
  if (floats.Count() != 0) {
            // preferred widths accumulated for floats that have already
            // been cleared past
    nscoord floats_done = 0,
            // preferred widths accumulated for floats that have not yet
            // been cleared past
            floats_cur_left = 0,
            floats_cur_right = 0;

    for (PRInt32 i = 0, i_end = floats.Count(); i != i_end; ++i) {
      nsIFrame *floatFrame = NS_STATIC_CAST(nsIFrame*, floats[i]);
      const nsStyleDisplay *floatDisp = floatFrame->GetStyleDisplay();
      if (floatDisp->mBreakType == NS_STYLE_CLEAR_LEFT ||
          floatDisp->mBreakType == NS_STYLE_CLEAR_RIGHT ||
          floatDisp->mBreakType == NS_STYLE_CLEAR_LEFT_AND_RIGHT) {
        nscoord floats_cur = floats_cur_left + floats_cur_right;
        if (floats_cur > floats_done)
          floats_done = floats_cur;
        if (floatDisp->mBreakType != NS_STYLE_CLEAR_RIGHT)
          floats_cur_left = 0;
        if (floatDisp->mBreakType != NS_STYLE_CLEAR_LEFT)
          floats_cur_right = 0;
      }

      nscoord &floats_cur = floatDisp->mFloats == NS_STYLE_FLOAT_LEFT
                              ? floats_cur_left : floats_cur_right;
      floats_cur +=
        nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                      floatFrame, nsLayoutUtils::PREF_WIDTH);
    }

    nscoord floats_cur = floats_cur_left + floats_cur_right;
    if (floats_cur > floats_done)
      floats_done = floats_cur;

    currentLine += floats_done;

    floats.Clear();
  }

  currentLine -= trailingWhitespace;
  prevLines = PR_MAX(prevLines, currentLine);
  currentLine = trailingWhitespace = 0;
}

static void
AddCoord(const nsStyleCoord& aStyle,
         nsIRenderingContext* aRenderingContext,
         nsIFrame* aFrame,
         nscoord* aCoord, float* aPercent)
{
  switch (aStyle.GetUnit()) {
    case eStyleUnit_Coord:
      *aCoord += aStyle.GetCoordValue();
      break;
    case eStyleUnit_Percent:
      *aPercent += aStyle.GetPercentValue();
      break;
    case eStyleUnit_Chars: {
      SetFontFromStyle(aRenderingContext, aFrame->GetStyleContext());
      nscoord fontWidth;
      aRenderingContext->SetTextRunRTL(PR_FALSE);
      aRenderingContext->GetWidth('M', fontWidth);
      *aCoord += aStyle.GetIntValue() * fontWidth;
      break;
    }
    default:
      break;
  }
}

/* virtual */ nsIFrame::IntrinsicWidthOffsetData
nsFrame::IntrinsicWidthOffsets(nsIRenderingContext* aRenderingContext)
{
  IntrinsicWidthOffsetData result;
  nsStyleCoord tmp;

  const nsStyleMargin *styleMargin = GetStyleMargin();
  AddCoord(styleMargin->mMargin.GetLeft(tmp), aRenderingContext, this,
           &result.hMargin, &result.hPctMargin);
  AddCoord(styleMargin->mMargin.GetRight(tmp), aRenderingContext, this,
           &result.hMargin, &result.hPctMargin);

  const nsStylePadding *stylePadding = GetStylePadding();
  AddCoord(stylePadding->mPadding.GetLeft(tmp), aRenderingContext, this,
           &result.hPadding, &result.hPctPadding);
  AddCoord(stylePadding->mPadding.GetRight(tmp), aRenderingContext, this,
           &result.hPadding, &result.hPctPadding);

  const nsStyleBorder *styleBorder = GetStyleBorder();
  result.hBorder += styleBorder->GetBorderWidth(NS_SIDE_LEFT);
  result.hBorder += styleBorder->GetBorderWidth(NS_SIDE_RIGHT);

  const nsStyleDisplay *disp = GetStyleDisplay();
  if (IsThemed(disp)) {
    nsPresContext *presContext = GetPresContext();

    nsMargin border;
    presContext->GetTheme()->GetWidgetBorder(presContext->DeviceContext(),
                                             this, disp->mAppearance,
                                             &border);
    result.hBorder = presContext->DevPixelsToAppUnits(border.LeftRight());

    nsMargin padding;
    if (presContext->GetTheme()->GetWidgetPadding(presContext->DeviceContext(),
                                                  this, disp->mAppearance,
                                                  &padding)) {
      result.hPadding = presContext->DevPixelsToAppUnits(padding.LeftRight());
      result.hPctPadding = 0;
    }
  }

  return result;
}

inline PRBool
IsAutoHeight(const nsStyleCoord &aCoord, nscoord aCBHeight)
{
  nsStyleUnit unit = aCoord.GetUnit();
  return unit == eStyleUnit_Auto ||  // only for 'height'
         unit == eStyleUnit_Null ||  // only for 'max-height'
         (unit == eStyleUnit_Percent && 
          aCBHeight == NS_AUTOHEIGHT);
}

/* virtual */ nsSize
nsFrame::ComputeSize(nsIRenderingContext *aRenderingContext,
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

  // Compute width

  if (stylePos->mWidth.GetUnit() != eStyleUnit_Auto) {
    result.width =
      nsLayoutUtils::ComputeWidthDependentValue(aRenderingContext, this,
        aCBSize.width, stylePos->mWidth) -
      boxSizingAdjust.width;
  }

  if (stylePos->mMaxWidth.GetUnit() != eStyleUnit_Null) {
    nscoord maxWidth =
      nsLayoutUtils::ComputeWidthDependentValue(aRenderingContext, this,
        aCBSize.width, stylePos->mMaxWidth) -
      boxSizingAdjust.width;
    if (maxWidth < result.width)
      result.width = maxWidth;
  }

  nscoord minWidth =
    nsLayoutUtils::ComputeWidthDependentValue(aRenderingContext, this,
      aCBSize.width, stylePos->mMinWidth) -
    boxSizingAdjust.width;
  if (minWidth > result.width)
    result.width = minWidth;

  // Compute height

  if (!IsAutoHeight(stylePos->mHeight, aCBSize.height)) {
    result.height =
      nsLayoutUtils::ComputeHeightDependentValue(aRenderingContext, this,
        aCBSize.height, stylePos->mHeight) -
      boxSizingAdjust.height;
  }

  if (result.height != NS_UNCONSTRAINEDSIZE) {
    if (!IsAutoHeight(stylePos->mMaxHeight, aCBSize.height)) {
      nscoord maxHeight =
        nsLayoutUtils::ComputeHeightDependentValue(aRenderingContext, this,
          aCBSize.height, stylePos->mMaxHeight) -
        boxSizingAdjust.height;
      if (maxHeight < result.height)
        result.height = maxHeight;
    }

    if (!IsAutoHeight(stylePos->mMinHeight, aCBSize.height)) {
      nscoord minHeight =
        nsLayoutUtils::ComputeHeightDependentValue(aRenderingContext, this,
          aCBSize.height, stylePos->mMinHeight) -
        boxSizingAdjust.height;
      if (minHeight > result.height)
        result.height = minHeight;
    }
  }

  const nsStyleDisplay *disp = GetStyleDisplay();
  if (IsThemed(disp)) {
    nsSize size(0, 0);
    PRBool canOverride = PR_TRUE;
    nsPresContext *presContext = GetPresContext();
    presContext->GetTheme()->
      GetMinimumWidgetSize(aRenderingContext, this, disp->mAppearance,
                           &size, &canOverride);

    size.width = presContext->DevPixelsToAppUnits(size.width);
    size.height = presContext->DevPixelsToAppUnits(size.height);

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

/* virtual */ nsSize
nsFrame::ComputeAutoSize(nsIRenderingContext *aRenderingContext,
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
nsFrame::ShrinkWidthToFit(nsIRenderingContext *aRenderingContext,
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

  // Notify the percent height observer if there is a percent height
  // but no computed height. The observer may be able to initiate
  // another reflow with a computed height. This happens in the case
  // where a table cell has no computed height but can fabricate one
  // when the cell height is known.
  if (aReflowState && aReflowState->mPercentHeightObserver &&
      ((NS_UNCONSTRAINEDSIZE == aReflowState->mComputedHeight) ||         // no computed height 
       (0                    == aReflowState->mComputedHeight))        && 
      (eStyleUnit_Percent == aReflowState->mStylePosition->mHeight.GetUnit())) {

    nsIFrame* prevInFlow = GetPrevInFlow();
    if (!prevInFlow) { // 1st in flow
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
nsFrame::TrimTrailingWhiteSpace(nsPresContext* aPresContext,
                                nsIRenderingContext& aRC,
                                nscoord& aDeltaWidth,
                                PRBool& aLastCharIsJustifiable)
{
  aDeltaWidth = 0;
  aLastCharIsJustifiable = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::CharacterDataChanged(nsPresContext* aPresContext,
                              nsIContent*     aChild,
                              PRBool          aAppend)
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

nsIView*
nsIFrame::GetParentViewForChildFrame(nsIFrame* aFrame) const
{
  return GetClosestView();
}

// Associated view object
nsIView*
nsIFrame::GetView() const
{
  // Check the frame state bit and see if the frame has a view
  if (!(GetStateBits() & NS_FRAME_HAS_VIEW))
    return nsnull;

  // Check for a property on the frame
  nsresult rv;
  void *value = GetProperty(nsGkAtoms::viewProperty, &rv);

  NS_ENSURE_SUCCESS(rv, nsnull);
  NS_ASSERTION(value, "frame state bit was set but frame has no view");
  return NS_STATIC_CAST(nsIView*, value);
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
    nsresult rv = SetProperty(nsGkAtoms::viewProperty, aView, nsnull);
    NS_ENSURE_SUCCESS(rv, rv);

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
  // Note that if we hit a view while walking up the frame tree we need to stop
  // and switch to traversing the view tree so that we will deal with scroll
  // views properly.
  nsPoint offset(0, 0);
  const nsIFrame* f;
  for (f = this; !f->HasView() && f != aOther; f = f->GetParent()) {
    offset += f->GetPosition();
  }
  
  if (f != aOther) {
    // We found a view.  Switch to the view tree
    nsPoint toViewOffset(0, 0);
    nsIView* otherView = aOther->GetClosestView(&toViewOffset);
    offset += f->GetView()->GetOffsetTo(otherView) - toViewOffset;
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
  nsIntRect retval(0,0,0,0);
  nsPoint toViewOffset(0,0);
  nsIView* view = GetClosestView(&toViewOffset);

  if (view) {
    nsPoint toWidgetOffset(0,0);
    nsIWidget* widget = view->GetNearestWidget(&toWidgetOffset);

    if (widget) {
      nsRect ourRect = mRect;
      ourRect.MoveTo(toViewOffset + toWidgetOffset);
      ourRect.ScaleRoundOut(1.0f / GetPresContext()->AppUnitsPerDevPixel());
      // Is it safe to pass the same rect for both args of WidgetToScreen?
      // It's not clear, so let's not...
      nsIntRect ourPxRect(ourRect.x, ourRect.y, ourRect.width, ourRect.height);
      
      widget->WidgetToScreen(ourPxRect, retval);
    }
  }

  return retval;
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

// The (x,y) value of the frame's upper left corner is always
// relative to its parentFrame's upper left corner, unless
// its parentFrame has a view associated with it, in which case, it
// will be relative to the upper left corner of the view returned
// by a call to parentFrame->GetView().
//
// This means that while drilling down the frame hierarchy, from
// parent to child frame, we sometimes need to take into account
// crossing these view boundaries, because the coordinate system
// changes from parent frame coordinate system, to the associated
// view's coordinate system.
//
// GetOriginToViewOffset() is a utility method that returns the
// offset necessary to map a point, relative to the frame's upper
// left corner, into the coordinate system of the view associated
// with the frame.
//
// If there is no view associated with the frame, or the view is
// not a descendant of the frame's parent view (ex: scrolling popup menu),
// the offset returned will be (0,0).

NS_IMETHODIMP nsFrame::GetOriginToViewOffset(nsPoint&        aOffset,
                                             nsIView**       aView) const
{
  nsresult rv = NS_OK;

  aOffset.MoveTo(0,0);

  if (aView)
    *aView = nsnull;

  if (HasView()) {
    nsIView *view = GetView();
    nsIView *parentView = nsnull;
    nsPoint offsetToParentView;
    rv = GetOffsetFromView(offsetToParentView, &parentView);

    if (NS_SUCCEEDED(rv)) {
      nsPoint viewOffsetFromParent(0,0);
      nsIView *pview = view;

      nsIViewManager* vVM = view->GetViewManager();

      while (pview && pview != parentView) {
        viewOffsetFromParent += pview->GetPosition();

        nsIView *tmpView = pview->GetParent();
        if (tmpView && vVM != tmpView->GetViewManager()) {
          // Don't cross ViewManager boundaries!
          // XXXbz why not?
          break;
        }
        pview = tmpView;
      }

#ifdef DEBUG_KIN
      if (pview != parentView) {
        // XXX: At this point, pview is probably null since it traversed
        //      all the way up view's parent hierarchy and did not run across
        //      parentView. In the future, instead of just returning an offset
        //      of (0,0) for this case, we may want offsetToParentView to
        //      include the offset from the parentView to the top of the
        //      view hierarchy which would make both offsetToParentView and
        //      viewOffsetFromParent, offsets to the global coordinate space.
        //      We'd have to investigate any perf impact this would have before
        //      checking in such a change, so for now we just return (0,0).
        //      -- kin    
        NS_WARNING("view is not a descendant of parentView!");
      }
#endif // DEBUG

      if (pview == parentView)
        aOffset = offsetToParentView - viewOffsetFromParent;

      if (aView)
        *aView = view;
    }
  }

  return rv;
}

/* virtual */ PRBool
nsIFrame::AreAncestorViewsVisible() const
{
  for (nsIView* view = GetClosestView(); view; view = view->GetParent()) {
    if (view->GetVisibility() == nsViewVisibility_kHide) {
      return PR_FALSE;
    }
  }
  return PR_TRUE;
}

nsIWidget*
nsIFrame::GetWindow() const
{
  return GetClosestView()->GetNearestWidget(nsnull);
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

void
nsIFrame::Invalidate(const nsRect& aDamageRect,
                     PRBool        aImmediate)
{
  if (aDamageRect.IsEmpty()) {
    return;
  }

  // Don't allow invalidates to do anything when
  // painting is suppressed.
  nsIPresShell *shell = GetPresContext()->GetPresShell();
  if (shell) {
    PRBool suppressed = PR_FALSE;
    shell->IsPaintingSuppressed(&suppressed);
    if (suppressed)
      return;
  }
  
  InvalidateInternal(aDamageRect, 0, 0, nsnull, aImmediate);
}

void
nsIFrame::InvalidateInternal(const nsRect& aDamageRect, nscoord aX, nscoord aY,
                             nsIFrame* aForChild, PRBool aImmediate)
{
  GetParent()->
    InvalidateInternal(aDamageRect, aX + mRect.x, aY + mRect.y, this, aImmediate);
}

void
nsIFrame::InvalidateRoot(const nsRect& aDamageRect,
                         nscoord aX, nscoord aY, PRBool aImmediate)
{
  PRUint32 flags = aImmediate ? NS_VMREFRESH_IMMEDIATE : NS_VMREFRESH_NO_SYNC;
  nsIView* view = GetView();
  NS_ASSERTION(view, "This can only be called on frames with views");
  view->GetViewManager()->UpdateView(view, aDamageRect + nsPoint(aX, aY), flags);
}

static nsRect ComputeOutlineRect(const nsIFrame* aFrame, PRBool* aAnyOutline,
                                 const nsRect& aOverflowRect) {
  const nsStyleOutline* outline = aFrame->GetStyleOutline();
  PRUint8 outlineStyle = outline->GetOutlineStyle();
  nsRect r = aOverflowRect;
  *aAnyOutline = PR_FALSE;
  if (outlineStyle != NS_STYLE_BORDER_STYLE_NONE) {
    nscoord width;
#ifdef DEBUG
    PRBool result = 
#endif
      outline->GetOutlineWidth(width);
    NS_ASSERTION(result, "GetOutlineWidth had no cached outline width");
    if (width > 0) {
      nscoord offset;
      outline->GetOutlineOffset(offset);
      nscoord inflateBy = PR_MAX(width + offset, 0);
      r.Inflate(inflateBy, inflateBy);
      *aAnyOutline = PR_TRUE;
    }
  }
  return r;
}

nsRect
nsIFrame::GetOverflowRect() const
{
  // Note that in some cases the overflow area might not have been
  // updated (yet) to reflect any outline set on the frame or the area
  // of child frames. That's OK because any reflow that updates these
  // areas will invalidate the appropriate area, so any (mis)uses of
  // this method will be fixed up.
  nsRect* storedOA = NS_CONST_CAST(nsIFrame*, this)
    ->GetOverflowAreaProperty(PR_FALSE);
  if (storedOA) {
    return *storedOA;
  } else {
    return nsRect(nsPoint(0, 0), GetSize());
  }
}
  
void
nsFrame::CheckInvalidateSizeChange(nsPresContext* aPresContext,
                                   nsHTMLReflowMetrics& aDesiredSize,
                                   const nsHTMLReflowState& aReflowState)
{
  if (aDesiredSize.width == mRect.width
      && aDesiredSize.height == mRect.height)
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

  // Invalidate the entire old frame+outline if the frame has an outline
  PRBool anyOutline;
  nsRect r = ComputeOutlineRect(this, &anyOutline,
                                aDesiredSize.mOverflowArea);
  if (anyOutline) {
    Invalidate(r);
    return;
  }

  // Invalidate the old frame borders if the frame has borders. Those borders
  // may be moving.
  const nsStyleBorder* border = GetStyleBorder();
  NS_FOR_CSS_SIDES(side) {
    if (border->GetBorderWidth(side) != 0) {
      Invalidate(nsRect(0, 0, mRect.width, mRect.height));
      return;
    }
  }

  // Invalidate the old frame background if the frame has a background
  // whose position depends on the size of the frame
  const nsStyleBackground* background = GetStyleBackground();
  if (background->mBackgroundFlags &
      (NS_STYLE_BG_X_POSITION_PERCENT | NS_STYLE_BG_Y_POSITION_PERCENT)) {
    Invalidate(nsRect(0, 0, mRect.width, mRect.height));
    return;
  }
}

// Define the MAX_FRAME_DEPTH to be the ContentSink's MAX_REFLOW_DEPTH plus
// 4 for the frames above the document's frames: 
//  the Viewport, GFXScroll, ScrollPort, and Canvas
#define MAX_FRAME_DEPTH (MAX_REFLOW_DEPTH+4)

PRBool
nsFrame::IsFrameTreeTooDeep(const nsHTMLReflowState& aReflowState,
                            nsHTMLReflowMetrics& aMetrics)
{
  if (aReflowState.mReflowDepth >  MAX_FRAME_DEPTH) {
    mState |= NS_FRAME_IS_UNFLOWABLE;
    mState &= ~NS_FRAME_OUTSIDE_CHILDREN;
    aMetrics.width = 0;
    aMetrics.height = 0;
    aMetrics.ascent = 0;
    aMetrics.mCarriedOutBottomMargin.Zero();
    aMetrics.mOverflowArea.x = 0;
    aMetrics.mOverflowArea.y = 0;
    aMetrics.mOverflowArea.width = 0;
    aMetrics.mOverflowArea.height = 0;
    return PR_TRUE;
  }
  mState &= ~NS_FRAME_IS_UNFLOWABLE;
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

#ifdef DEBUG_waterson

/**
 * List a single frame to stdout. Meant to be called from gdb.
 */
void
DebugListFrame(nsPresContext* aPresContext, nsIFrame* aFrame)
{
  ((nsFrame*) aFrame)->List(stdout, 0);
  printf("\n");
}

/**
 * List a frame tree to stdout. Meant to be called from gdb.
 */
void
DebugListFrameTree(nsPresContext* aPresContext, nsIFrame* aFrame)
{
  nsIFrameDebug* fdbg;
  aFrame->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**) &fdbg);
  if (fdbg)
    fdbg->List(stdout, 0);
}

#endif


// Debugging
NS_IMETHODIMP
nsFrame::List(FILE* out, PRInt32 aIndent) const
{
  IndentBy(out, aIndent);
  ListTag(out);
#ifdef DEBUG_waterson
  fprintf(out, " [parent=%p]", NS_STATIC_CAST(void*, mParent));
#endif
  if (HasView()) {
    fprintf(out, " [view=%p]", NS_STATIC_CAST(void*, GetView()));
  }
  fprintf(out, " {%d,%d,%d,%d}", mRect.x, mRect.y, mRect.width, mRect.height);
  if (0 != mState) {
    fprintf(out, " [state=%08x]", mState);
  }
  nsIFrame* prevInFlow = GetPrevInFlow();
  nsIFrame* nextInFlow = GetNextInFlow();
  if (nsnull != prevInFlow) {
    fprintf(out, " prev-in-flow=%p", NS_STATIC_CAST(void*, prevInFlow));
  }
  if (nsnull != nextInFlow) {
    fprintf(out, " next-in-flow=%p", NS_STATIC_CAST(void*, nextInFlow));
  }
  fprintf(out, " [content=%p]", NS_STATIC_CAST(void*, mContent));
  nsFrame* f = NS_CONST_CAST(nsFrame*, this);
  nsRect* overflowArea = f->GetOverflowAreaProperty(PR_FALSE);
  if (overflowArea) {
    fprintf(out, " [overflow=%d,%d,%d,%d]", overflowArea->x, overflowArea->y,
            overflowArea->width, overflowArea->height);
  }
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
nsFrame::ParentDisablesSelection() const
{
/*
  // should never be called now
  nsIFrame* parent = GetParent();
  if (parent) {
	  PRBool selectable;
	  parent->IsSelectable(selectable);
    return (selectable ? PR_FALSE : PR_TRUE);
  }
  return PR_FALSE;
*/
/*
  PRBool selected;
  if (NS_FAILED(GetSelected(&selected)))
    return PR_FALSE;
  if (selected)
    return PR_FALSE; //if this frame is selected and no one has overridden the selection from "higher up"
                     //then no one below us will be disabled by this frame.
  nsIFrame* target = GetParent();
  if (target)
    return ((nsFrame *)target)->ParentDisablesSelection();
  return PR_FALSE; //default this does not happen
  */
  
  return PR_FALSE;
}

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

  nsPresContext* pc = GetPresContext();
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
    nsITextControlFrame *tcf;
    if (NS_SUCCEEDED(frame->QueryInterface(NS_GET_IID(nsITextControlFrame),(void**)&tcf))) {
      NS_IF_ADDREF(*aSelCon = tcf->GetOwnedSelectionController());
      return NS_OK;
    }
    frame = frame->GetParent();
  }

  return CallQueryInterface(aPresContext->GetPresShell(), aSelCon);
}

nsFrameSelection* nsIFrame::GetFrameSelection()
{
  nsIFrame *frame = this;
  while (frame && (frame->GetStateBits() & NS_FRAME_INDEPENDENT_SELECTION)) {
    nsITextControlFrame *tcf;
    if (NS_SUCCEEDED(frame->QueryInterface(NS_GET_IID(nsITextControlFrame),(void**)&tcf))) {
      return tcf->GetOwnedFrameSelection();
    }
    frame = frame->GetParent();
  }

  return GetPresContext()->PresShell()->FrameSelection();
}

#ifdef NS_DEBUG
NS_IMETHODIMP
nsFrame::DumpRegressionData(nsPresContext* aPresContext, FILE* out, PRInt32 aIndent, PRBool aIncludeStyleData)
{
  IndentBy(out, aIndent);
  fprintf(out, "<frame va=\"%ld\" type=\"", PRUptrdiff(this));
  nsAutoString name;
  GetFrameName(name);
  XMLQuote(name);
  fputs(NS_LossyConvertUTF16toASCII(name).get(), out);
  fprintf(out, "\" state=\"%d\" parent=\"%ld\">\n",
          GetDebugStateBits(), PRUptrdiff(mParent));

  aIndent++;
  DumpBaseRegressionData(aPresContext, out, aIndent, aIncludeStyleData);
  aIndent--;

  IndentBy(out, aIndent);
  fprintf(out, "</frame>\n");

  return NS_OK;
}

void
nsFrame::DumpBaseRegressionData(nsPresContext* aPresContext, FILE* out, PRInt32 aIndent, PRBool aIncludeStyleData)
{
  if (nsnull != mNextSibling) {
    IndentBy(out, aIndent);
    fprintf(out, "<next-sibling va=\"%ld\"/>\n", PRUptrdiff(mNextSibling));
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

  if(aIncludeStyleData) {
    if(mStyleContext) {
      IndentBy(out, aIndent);
      fprintf(out, "<stylecontext va=\"%ld\">\n", PRUptrdiff(mStyleContext));
      aIndent++;
      // Dump style context regression data
      mStyleContext->DumpRegressionData(aPresContext, out, aIndent);
      aIndent--;
      IndentBy(out, aIndent);
      fprintf(out, "</stylecontext>\n");
    }
  }

  IndentBy(out, aIndent);
  fprintf(out, "<bbox x=\"%d\" y=\"%d\" w=\"%d\" h=\"%d\"/>\n",
          mRect.x, mRect.y, mRect.width, mRect.height);

  // Now dump all of the children on all of the child lists
  nsIFrame* kid;
  nsIAtom* list = nsnull;
  PRInt32 listIndex = 0;
  do {
    kid = GetFirstChild(list);
    if (kid) {
      IndentBy(out, aIndent);
      if (nsnull != list) {
        nsAutoString listName;
        list->ToString(listName);
        fprintf(out, "<child-list name=\"");
        XMLQuote(listName);
        fputs(NS_LossyConvertUTF16toASCII(listName).get(), out);
        fprintf(out, "\">\n");
      }
      else {
        fprintf(out, "<child-list>\n");
      }
      aIndent++;
      while (kid) {
        nsIFrameDebug*  frameDebug;

        if (NS_SUCCEEDED(kid->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**)&frameDebug))) {
          frameDebug->DumpRegressionData(aPresContext, out, aIndent, aIncludeStyleData);
        }
        kid = kid->GetNextSibling();
      }
      aIndent--;
      IndentBy(out, aIndent);
      fprintf(out, "</child-list>\n");
    }
    list = GetAdditionalChildListName(listIndex++);
  } while (nsnull != list);
}

NS_IMETHODIMP
nsFrame::VerifyTree() const
{
  NS_ASSERTION(0 == (mState & NS_FRAME_IN_REFLOW), "frame is in reflow");
  return NS_OK;
}
#endif

/*this method may.. invalidate if the state was changed or if aForceRedraw is PR_TRUE
  it will not update immediately.*/
NS_IMETHODIMP
nsFrame::SetSelected(nsPresContext* aPresContext, nsIDOMRange *aRange, PRBool aSelected, nsSpread aSpread)
{
/*
  if (aSelected && ParentDisablesSelection())
    return NS_OK;
*/

  // check whether style allows selection
  PRBool  selectable;
  IsSelectable(&selectable, nsnull);
  if (!selectable)
    return NS_OK;

/*
  if (eSpreadDown == aSpread){
    nsIFrame* kid = GetFirstChild(nsnull);
    while (nsnull != kid) {
      kid->SetSelected(nsnull,aSelected,aSpread);
      kid = kid->GetNextSibling();
    }
  }
*/
  if ( aSelected ){
    AddStateBits(NS_FRAME_SELECTED_CONTENT);
  }
  else
    RemoveStateBits(NS_FRAME_SELECTED_CONTENT);

  // Repaint this frame subtree's entire area
  Invalidate(GetOverflowRect(), PR_FALSE);

#ifdef IBMBIDI
  PRInt32 start, end;
  nsIFrame* frame = GetNextSibling();
  if (frame) {
    GetFirstLeaf(aPresContext, &frame);
    GetOffsets(start, end);
    if (start && end) {
      frame->SetSelected(aPresContext, aRange, aSelected, aSpread);
    }
  }
#endif // IBMBIDI

  return NS_OK;
}

NS_IMETHODIMP
nsFrame::GetSelected(PRBool *aSelected) const
{
  if (!aSelected )
    return NS_ERROR_NULL_POINTER;
  *aSelected = (PRBool)(mState & NS_FRAME_SELECTED_CONTENT);
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::GetPointFromOffset(nsPresContext* inPresContext, nsIRenderingContext* inRendContext, PRInt32 inOffset, nsPoint* outPoint)
{
  NS_PRECONDITION(outPoint != nsnull, "Null parameter");
  nsPoint bottomLeft(0, 0);
  if (mContent)
  {
    nsIContent* newContent = mContent->GetParent();
    if (newContent){
      PRInt32 newOffset = newContent->IndexOf(mContent);

      PRBool isRTL = (NS_GET_EMBEDDING_LEVEL(this) & 1) == 1;
      if (!isRTL && inOffset > newOffset ||
          isRTL && inOffset <= newOffset)
        bottomLeft.x = GetRect().width;
    }
  }
  *outPoint = bottomLeft;
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

   nsresult result;
  nsCOMPtr<nsILineIteratorNavigator> it; 
  result = aBlockFrame->QueryInterface(NS_GET_IID(nsILineIteratorNavigator),getter_AddRefs(it));
  if (NS_FAILED(result) || !it)
    return result;
  PRInt32 searchingLine = aLineStart;
  PRInt32 countLines;
  result = it->GetNumLines(&countLines);
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
      nsCOMPtr<nsILineIteratorNavigator> newIt; 
      //check to see if this is ANOTHER blockframe inside the other one if so then call into its lines
      result = resultFrame->QueryInterface(NS_GET_IID(nsILineIteratorNavigator),getter_AddRefs(newIt));
      if (NS_SUCCEEDED(result) && newIt)
      {
        aPos->mResultFrame = resultFrame;
        return NS_OK;
      }
      //resultFrame is not a block frame

      nsCOMPtr<nsIBidirectionalEnumerator> frameTraversal;
      result = NS_NewFrameTraversal(getter_AddRefs(frameTraversal),
                                    aPresContext, resultFrame,
                                    ePostOrder,
                                    PR_FALSE, // aVisual
                                    aPos->mScrollViewStop,
                                    PR_FALSE  // aFollowOOFs
                                    );
      if (NS_FAILED(result))
        return result;
      nsISupports *isupports = nsnull;
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
        PRInt16 isEditor = 0;
        nsIPresShell *shell = aPresContext->GetPresShell();
        if (!shell)
          return NS_ERROR_FAILURE;
        shell->GetSelectionFlags ( &isEditor );
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
        result = frameTraversal->Prev();
        if (NS_FAILED(result))
          break;
        result = frameTraversal->CurrentItem(&isupports);
        if (NS_FAILED(result) || !isupports)
          return result;
        //we must CAST here to an nsIFrame. nsIFrame doesnt really follow the rules
        resultFrame = (nsIFrame *)isupports;
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
        result = frameTraversal->Next();
        if (NS_FAILED(result))
          break;
        result = frameTraversal->CurrentItem(&isupports);
        if (NS_FAILED(result) || !isupports)
          break;
        //we must CAST here to an nsIFrame. nsIFrame doesnt really follow the rules
        resultFrame = (nsIFrame *)isupports;
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

  if (aFrame->IsGeneratedContentFrame())
    return result;

  // Treat form controls as inline leaves
  // XXX we really need a way to determine whether a frame is inline-level
  nsIFormControlFrame* fcf; // used only for QI
  nsresult rv = aFrame->QueryInterface(NS_GET_IID(nsIFormControlFrame), (void**)&fcf);
  if (NS_SUCCEEDED(rv))
    return result;
  
  // Check the frame itself
  nsBlockFrame* bf; // used only for QI
  rv = aFrame->QueryInterface(kBlockFrameCID, (void**)&bf);
  // Fall through "special" block frames because their mContent is the content
  // of the inline frames they were created from. The first/last child of
  // such frames is the real block frame we're looking for.
  if (NS_SUCCEEDED(rv) && !(aFrame->GetStateBits() & NS_FRAME_IS_SPECIAL) ||
      aFrame->GetType() == nsGkAtoms::brFrame) {
    nsIContent* content = aFrame->GetContent();
    result.mContent = content->GetParent();
    result.mOffset = result.mContent->IndexOf(content) + 
      (aDirection == eDirPrevious ? 1 : 0);
    return result;
  }

  // If this is a preformatted text frame, see if it ends with a newline
  if (aFrame->HasTerminalNewline() &&
      aFrame->GetStyleContext()->GetStyleText()->WhiteSpaceIsSignificant()) {
    PRInt32 startOffset, endOffset;
    aFrame->GetOffsets(startOffset, endOffset);
    result.mContent = aFrame->GetContent();
    result.mOffset = endOffset - (aDirection == eDirPrevious ? 0 : 1);
    return result;
  }

  // Iterate over children and call ourselves recursively
  if (aDirection == eDirPrevious) {
    nsFrameList children(aFrame->GetFirstChild(nsnull));
    nsIFrame* child = children.LastChild();
    while(child && !result.mContent) {
      result = FindBlockFrameOrBR(child, aDirection);
      child = children.GetPrevSiblingFor(child);
    }
  } else { // eDirNext
    nsIFrame* child = aFrame->GetFirstChild(nsnull);
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
  nsBlockFrame* bf;  // used only for QI
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
      if (!parent) { // Treat the root frame as if it were a block frame.
        reachedBlockAncestor = PR_TRUE;
        break;
      }
      nsFrameList siblings(parent->GetFirstChild(nsnull));
      nsIFrame* sibling = siblings.GetPrevSiblingFor(frame);
      while (sibling && !blockFrameOrBR.mContent) {
        blockFrameOrBR = FindBlockFrameOrBR(sibling, eDirPrevious);
        sibling = siblings.GetPrevSiblingFor(sibling);
      }
      if (blockFrameOrBR.mContent) {
        aPos->mResultContent = blockFrameOrBR.mContent;
        aPos->mContentOffset = blockFrameOrBR.mOffset;
        break;
      }
      frame = parent;
      reachedBlockAncestor = NS_SUCCEEDED(frame->QueryInterface(kBlockFrameCID, (void**)&bf));
    }
    if (reachedBlockAncestor) { // no "stop frame" found
      aPos->mResultContent = frame->GetContent();
      aPos->mContentOffset = 0;
    }
  } else { // eDirNext
    while (!reachedBlockAncestor) {
      nsIFrame* parent = frame->GetParent();
      if (!parent) { // Treat the root frame as if it were a block frame.
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
      reachedBlockAncestor = NS_SUCCEEDED(frame->QueryInterface(kBlockFrameCID, (void**)&bf));
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
    {
      PRBool eatingNonRenderableWS = PR_FALSE;
      PRBool done = PR_FALSE;
      
      while (!done) {
        PRBool movingInFrameDirection =
          IsMovingInFrameDirection(current, aPos->mDirection, aPos->mVisual);

        if (eatingNonRenderableWS)
          done = current->PeekOffsetNoAmount(movingInFrameDirection, &offset); 
        else
          done = current->PeekOffsetCharacter(movingInFrameDirection, &offset); 

        if (!done) {
          PRBool jumpedLine;
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
      
      break;
    }
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
        nsTextTransformer::Initialize();
        wordSelectEatSpace = aPos->mDirection == eDirNext && nsTextTransformer::GetWordSelectEatSpaceAfter();
      }      
      
      // sawBeforeType means "we already saw characters of the type
      // before the boundary we're looking for". Examples:
      // 1. If we're moving forward, looking for a word beginning (i.e. a boundary
      //    between whitespace and non-whitespace), then eatingWS==PR_TRUE means
      //    "we already saw some whitespace".
      // 2. If we're moving backward, looking for a word beginning (i.e. a boundary
      //    between non-whitespace and whitespace), then eatingWS==PR_TRUE means
      //    "we already saw some non-whitespace".
      PRBool sawBeforeType = PR_FALSE;

      PRBool done = PR_FALSE;
      while (!done) {
        PRBool movingInFrameDirection =
          IsMovingInFrameDirection(current, aPos->mDirection, aPos->mVisual);
        
        done = current->PeekOffsetWord(movingInFrameDirection, wordSelectEatSpace, aPos->mIsKeyboardSelect,
                                       &offset, &sawBeforeType);
        
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
              jumpedLine && !wordSelectEatSpace && sawBeforeType) {
            done = PR_TRUE;
          } else {
            current = nextFrame;
            offset = nextFrameOffset;
            // Jumping a line is equivalent to encountering whitespace
            if (wordSelectEatSpace && jumpedLine)
              sawBeforeType = PR_TRUE;
          }
        }
      }
      
      // Set outputs
      range = GetRangeForFrame(current);
      aPos->mResultFrame = current;
      aPos->mResultContent = range.content;
      // Output offset is relative to content, not frame
      aPos->mContentOffset = offset < 0 ? range.end : range.start + offset;
      break;
    }
    case eSelectLine :
    {
      nsCOMPtr<nsILineIteratorNavigator> iter; 
      nsIFrame *blockFrame = this;

      while (NS_FAILED(result)){
        PRInt32 thisLine = nsFrame::GetLineNumber(blockFrame, &blockFrame);
        if (thisLine < 0) 
          return  NS_ERROR_FAILURE;
        result = blockFrame->QueryInterface(NS_GET_IID(nsILineIteratorNavigator),getter_AddRefs(iter));
        NS_ASSERTION(NS_SUCCEEDED(result) && iter, "GetLineNumber() succeeded but no block frame?");

        int edgeCase = 0;//no edge case. this should look at thisLine
        
        PRBool doneLooping = PR_FALSE;//tells us when no more block frames hit.
        //this part will find a frame or a block frame. if it's a block frame
        //it will "drill down" to find a viable frame or it will return an error.
        nsIFrame *lastFrame = this;
        do {
          result = nsFrame::GetNextPrevLineFromeBlockFrame(GetPresContext(),
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
              nsIFrame *frame = aPos->mResultFrame->GetFirstChild(nsnull);
              //got the table frame now
              while(frame) //ok time to drill down to find iterator
              {
                result = frame->QueryInterface(NS_GET_IID(nsILineIteratorNavigator),
                                                          getter_AddRefs(iter));
                if (NS_SUCCEEDED(result))
                {
                  aPos->mResultFrame = frame;
                  searchTableBool = PR_TRUE;
                  break; //while(frame)
                }
                frame = frame->GetFirstChild(nsnull);
              }
            }
            if (!searchTableBool)
              result = aPos->mResultFrame->QueryInterface(NS_GET_IID(nsILineIteratorNavigator),
                                                        getter_AddRefs(iter));
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
      nsCOMPtr<nsILineIteratorNavigator> it;
      // Adjusted so that the caret can't get confused when content changes
      nsIFrame* blockFrame = AdjustFrameForSelectionStyles(this);
      PRInt32 thisLine = nsFrame::GetLineNumber(blockFrame, &blockFrame);
      if (thisLine < 0)
        return NS_ERROR_FAILURE;
      result = blockFrame->QueryInterface(NS_GET_IID(nsILineIteratorNavigator),getter_AddRefs(it));
      NS_ASSERTION(NS_SUCCEEDED(result) && it, "GetLineNumber() succeeded but no block frame?");

      PRInt32 lineFrameCount;
      nsIFrame *firstFrame;
      nsRect usedRect;
      PRUint32 lineFlags;
      nsIFrame* baseFrame = nsnull;
      PRBool endOfLine = (eSelectEndLine == aPos->mAmount);
      
#ifdef IBMBIDI
      if (aPos->mVisual && GetPresContext()->BidiEnabled()) {
        PRBool lineIsRTL;
        it->GetDirection(&lineIsRTL);
        PRBool isReordered;
        nsIFrame *lastFrame;
        result = it->CheckLineOrder(thisLine, &isReordered, &firstFrame, &lastFrame);
        baseFrame = endOfLine ? lastFrame : firstFrame;
        nsBidiLevel embeddingLevel = nsBidiPresUtils::GetFrameEmbeddingLevel(baseFrame);
        // If the direction of the frame on the edge is opposite to that of the line,
        // we'll need to drill down to its opposite end, so reverse endOfLine.
        if ((embeddingLevel & 1) == !lineIsRTL)
          endOfLine = !endOfLine;
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
nsFrame::PeekOffsetCharacter(PRBool aForward, PRInt32* aOffset)
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
                        PRInt32* aOffset, PRBool* aSawBeforeType)
{
  NS_ASSERTION (aOffset && *aOffset <= 1, "aOffset out of range");
  PRInt32 startOffset = *aOffset;
  if (startOffset < 0)
    startOffset = 1;
  if (aForward == (startOffset == 0)) {
    // We're before the frame and moving forward, or after it and moving backwards.
    // If we're looking for non-whitespace, we found it (without skipping this frame).
    if (aWordSelectEatSpace && *aSawBeforeType)
      return PR_TRUE;
    // Otherwise skip to the other side and note that we encountered non-whitespace.
    *aOffset = 1 - startOffset;
    if (!aWordSelectEatSpace)
      *aSawBeforeType = PR_TRUE;
  }
  return PR_FALSE;
}

NS_IMETHODIMP
nsFrame::CheckVisibility(nsPresContext* , PRInt32 , PRInt32 , PRBool , PRBool *, PRBool *)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


PRInt32
nsFrame::GetLineNumber(nsIFrame *aFrame, nsIFrame** aContainingBlock)
{
  NS_ASSERTION(aFrame, "null aFrame");
  nsFrameManager* frameManager = aFrame->GetPresContext()->FrameManager();
  nsIFrame *blockFrame = aFrame;
  nsIFrame *thisBlock;
  PRInt32   thisLine;
  nsCOMPtr<nsILineIteratorNavigator> it; 
  nsresult result = NS_ERROR_FAILURE;
  while (NS_FAILED(result) && blockFrame)
  {
    thisBlock = blockFrame;
    if (thisBlock->GetStateBits() & NS_FRAME_OUT_OF_FLOW) {
      //if we are searching for a frame that is not in flow we will not find it. 
      //we must instead look for its placeholder
      thisBlock = frameManager->GetPlaceholderFrameFor(thisBlock);
      if (!thisBlock)
        return -1;
    }  
    blockFrame = thisBlock->GetParent();
    result = NS_OK;
    if (blockFrame) {
      result = blockFrame->QueryInterface(NS_GET_IID(nsILineIteratorNavigator),getter_AddRefs(it));
    }
  }
  if (!blockFrame || !it)
    return -1;

  if (aContainingBlock)
    *aContainingBlock = blockFrame;
  result = it->FindLineContaining(thisBlock, &thisLine);
  if (NS_FAILED(result))
    return -1;
  return thisLine;
}

nsresult
nsIFrame::GetFrameFromDirection(nsDirection aDirection, PRBool aVisual,
                                PRBool aJumpLines, PRBool aScrollViewStop, 
                                nsIFrame** aOutFrame, PRInt32* aOutOffset, PRBool* aOutJumpedLine)
{  
  if (!aOutFrame || !aOutOffset || !aOutJumpedLine)
    return NS_ERROR_NULL_POINTER;
  
  nsPresContext* presContext = GetPresContext();
  *aOutFrame = nsnull;
  *aOutOffset = 0;
  *aOutJumpedLine = PR_FALSE;

  // Find the prev/next selectable frame
  PRBool selectable = PR_FALSE;
  nsIFrame *traversedFrame = this;
  while (!selectable) {
    nsIFrame *blockFrame;
    nsCOMPtr<nsILineIteratorNavigator> it; 
    
    PRInt32 thisLine = nsFrame::GetLineNumber(traversedFrame, &blockFrame);
    if (thisLine < 0)
      return NS_ERROR_FAILURE;
    nsresult result = blockFrame->QueryInterface(NS_GET_IID(nsILineIteratorNavigator),getter_AddRefs(it));
    NS_ASSERTION(NS_SUCCEEDED(result) && it, "GetLineNumber() succeeded but no block frame?");

    PRBool atLineEdge;
    nsIFrame *firstFrame;
    nsIFrame *lastFrame;
#ifdef IBMBIDI
    if (aVisual && presContext->BidiEnabled()) {
      PRBool lineIsRTL;                                                             
      it->GetDirection(&lineIsRTL);
      PRBool isReordered;
      result = it->CheckLineOrder(thisLine, &isReordered, &firstFrame, &lastFrame);
      nsIFrame** framePtr = aDirection == eDirPrevious ? &firstFrame : &lastFrame;
      if (*framePtr) {
        nsBidiLevel embeddingLevel = nsBidiPresUtils::GetFrameEmbeddingLevel(*framePtr);
        if (((embeddingLevel & 1) && lineIsRTL || !(embeddingLevel & 1) && !lineIsRTL) ==
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
            NS_ASSERTION(0,"should not be reached nsFrame\n");
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

    nsCOMPtr<nsIBidirectionalEnumerator> frameTraversal;
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
      result = frameTraversal->Next();
    else
      result = frameTraversal->Prev();
    if (NS_FAILED(result))
      return result;

    nsISupports *isupports = nsnull;
    result = frameTraversal->CurrentItem(&isupports);
    if (NS_FAILED(result))
      return result;
    if (!isupports)
      return NS_ERROR_NULL_POINTER;
    //we must CAST here to an nsIFrame. nsIFrame doesn't really follow the rules
    //for speed reasons
    traversedFrame = (nsIFrame *)isupports;
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
NS_IMETHODIMP
nsFrame::GetAccessible(nsIAccessible** aAccessible)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
#endif

// Destructor function for the overflow area property
static void
DestroyRectFunc(void*    aFrame,
                nsIAtom* aPropertyName,
                void*    aPropertyValue,
                void*    aDtorData)
{
  delete NS_STATIC_CAST(nsRect*, aPropertyValue);
}

nsRect*
nsIFrame::GetOverflowAreaProperty(PRBool aCreateIfNecessary) 
{
  if (!((GetStateBits() & NS_FRAME_OUTSIDE_CHILDREN) || aCreateIfNecessary)) {
    return nsnull;
  }

  nsPropertyTable *propTable = GetPresContext()->PropertyTable();
  void *value = propTable->GetProperty(this,
                                       nsGkAtoms::overflowAreaProperty);

  if (value) {
    return (nsRect*)value;  // the property already exists
  } else if (aCreateIfNecessary) {
    // The property isn't set yet, so allocate a new rect, set the property,
    // and return the newly allocated rect
    nsRect*  overflow = new nsRect(0, 0, 0, 0);
    propTable->SetProperty(this, nsGkAtoms::overflowAreaProperty,
                           overflow, DestroyRectFunc, nsnull);
    return overflow;
  }

  NS_NOTREACHED("Frame abuses NS_FRAME_OUTSIDE_CHILDREN flag");
  return nsnull;
}

void 
nsIFrame::FinishAndStoreOverflow(nsRect* aOverflowArea, nsSize aNewSize)
{
  // This is now called FinishAndStoreOverflow() instead of 
  // StoreOverflow() because frame-generic ways of adding overflow
  // can happen here, e.g. CSS2 outline and native theme.
  // If we find more things other than outline that need to be added,
  // we should think about starting a new method like GetAdditionalOverflow()
  NS_ASSERTION(aNewSize.width == 0 || aNewSize.height == 0 ||
               aOverflowArea->Contains(nsRect(nsPoint(0, 0), aNewSize)),
               "Computed overflow area must contain frame bounds");

  const nsStyleDisplay *disp = GetStyleDisplay();
  if (IsThemed(disp)) {
    nsRect r;
    nsPresContext *presContext = GetPresContext();
    if (presContext->GetTheme()->
          GetWidgetOverflow(presContext->DeviceContext(), this,
                            disp->mAppearance, &r)) {
      aOverflowArea->UnionRect(*aOverflowArea, r);
    }
  }

  PRBool geometricOverflow =
    aOverflowArea->x < 0 || aOverflowArea->y < 0 ||
    aOverflowArea->XMost() > aNewSize.width || aOverflowArea->YMost() > aNewSize.height;
  // Clear geometric overflow area if we clip our children
  NS_ASSERTION((GetStyleDisplay()->mOverflowY == NS_STYLE_OVERFLOW_CLIP) ==
               (GetStyleDisplay()->mOverflowX == NS_STYLE_OVERFLOW_CLIP),
               "If one overflow is clip, the other should be too");
  if (geometricOverflow &&
      GetStyleDisplay()->mOverflowX == NS_STYLE_OVERFLOW_CLIP) {
    *aOverflowArea = nsRect(nsPoint(0, 0), aNewSize);
    geometricOverflow = PR_FALSE;
  }

  PRBool hasOutline;
  nsRect outlineRect(ComputeOutlineRect(this, &hasOutline, *aOverflowArea));

  if (hasOutline || geometricOverflow) {
    // Throw out any overflow if we're -moz-hidden-unscrollable
    mState |= NS_FRAME_OUTSIDE_CHILDREN;
    nsRect* overflowArea = GetOverflowAreaProperty(PR_TRUE); 
    NS_ASSERTION(overflowArea, "should have created rect");
    *aOverflowArea = *overflowArea = outlineRect;
  } 
  else {
    if (mState & NS_FRAME_OUTSIDE_CHILDREN) {
      // remove the previously stored overflow area 
      DeleteProperty(nsGkAtoms::overflowAreaProperty);
    }
    mState &= ~NS_FRAME_OUTSIDE_CHILDREN;
  }   
}

void
nsFrame::ConsiderChildOverflow(nsRect&   aOverflowArea,
                               nsIFrame* aChildFrame)
{
  const nsStyleDisplay* disp = GetStyleDisplay();
  // check here also for hidden as table frames (table, tr and td) currently 
  // don't wrap their content into a scrollable frame if overflow is specified
  if (!disp->IsTableClip()) {
    nsRect* overflowArea = aChildFrame->GetOverflowAreaProperty();
    if (overflowArea) {
      nsRect childOverflow(*overflowArea);
      childOverflow.MoveBy(aChildFrame->GetPosition());
      aOverflowArea.UnionRect(aOverflowArea, childOverflow);
    }
    else {
      aOverflowArea.UnionRect(aOverflowArea, aChildFrame->GetRect());
    }
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
 * This function takes a "special" frame and _if_ that frame is the
 * anonymous block crated by an ib split it returns the split inline
 * as aSpecialSibling.  This is needed because the split inline's
 * style context is the parent of the anonymous block's srtyle context.
 *
 * If aFrame is not the anonymous block, aSpecialSibling is not
 * touched.
 */
static nsresult
GetIBSpecialSibling(nsPresContext* aPresContext,
                    nsIFrame* aFrame,
                    nsIFrame** aSpecialSibling)
{
  NS_PRECONDITION(aFrame, "Must have a non-null frame!");
  NS_ASSERTION(aFrame->GetStateBits() & NS_FRAME_IS_SPECIAL,
               "GetIBSpecialSibling should not be called on a non-special frame");
  
  // Find the first-in-flow of the frame.  (Ugh.  This ends up
  // being O(N^2) when it is called O(N) times.)
  aFrame = aFrame->GetFirstInFlow();

  /*
   * Now look up the nsGkAtoms::IBSplitSpecialPrevSibling
   * property, which is only set on the anonymous block frames we're
   * interested in.
   */
  nsresult rv;
  nsIFrame *specialSibling = NS_STATIC_CAST(nsIFrame*,
                             aPresContext->PropertyTable()->GetProperty(aFrame,
                               nsGkAtoms::IBSplitSpecialPrevSibling, &rv));

  if (NS_OK == rv) {
    NS_ASSERTION(specialSibling, "null special sibling");
    *aSpecialSibling = specialSibling;
  }

  return NS_OK;
}

static PRBool
IsTablePseudo(nsIAtom* aPseudo)
{
  return
    aPseudo == nsCSSAnonBoxes::tableOuter ||
    aPseudo == nsCSSAnonBoxes::table ||
    aPseudo == nsCSSAnonBoxes::tableRowGroup ||
    aPseudo == nsCSSAnonBoxes::tableRow ||
    aPseudo == nsCSSAnonBoxes::tableCell ||
    aPseudo == nsCSSAnonBoxes::tableColGroup ||
    aPseudo == nsCSSAnonBoxes::tableCol;
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
  *aSpecialParent = parent;
  if (parent) {
    nsIAtom* pseudo = aFrame->GetStyleContext()->GetPseudoType();

    // if this frame itself is not scrolled-content, then skip any scrolled-content
    // parents since they're basically anonymous as far as the style system goes
    if (pseudo != nsCSSAnonBoxes::scrolledContent) {
      while (parent->GetStyleContext()->GetPseudoType() ==
             nsCSSAnonBoxes::scrolledContent) {
        parent = parent->GetParent();
      }
    }

    // If the frame is not a table pseudo frame, we want to move up
    // the tree till we get to a non-table-pseudo frame.
    if (!IsTablePseudo(pseudo)) {
      while (IsTablePseudo(parent->GetStyleContext()->GetPseudoType())) {
        parent = parent->GetParent();
      }
    }

    if (parent->GetStateBits() & NS_FRAME_IS_SPECIAL) {
      GetIBSpecialSibling(aPresContext, parent, aSpecialParent);
    } else {
      *aSpecialParent = parent;
    }
  }

  return NS_OK;
}

nsresult
nsFrame::DoGetParentStyleContextFrame(nsPresContext* aPresContext,
                                      nsIFrame**      aProviderFrame,
                                      PRBool*         aIsChild)
{
  *aIsChild = PR_FALSE;
  *aProviderFrame = nsnull;
  if (mContent && !mContent->GetParent() &&
      !GetStyleContext()->GetPseudoType()) {
    // we're a frame for the root.  We have no style context parent.
    return NS_OK;
  }
  
  if (!(mState & NS_FRAME_OUT_OF_FLOW)) {
    /*
     * If this frame is the anonymous block created when an inline
     * with a block inside it got split, then the parent style context
     * is on the first of the three special frames.  We can get to it
     * using GetIBSpecialSibling
     */
    if (mState & NS_FRAME_IS_SPECIAL) {
      GetIBSpecialSibling(aPresContext, this, aProviderFrame);
      if (*aProviderFrame)
        return NS_OK;
    }

    // If this frame is one of the blocks that split an inline, we must
    // return the "special" inline parent, i.e., the parent that this
    // frame would have if we didn't mangle the frame structure.
    return GetCorrectedParent(aPresContext, this, aProviderFrame);
  }

  // For out-of-flow frames, we must resolve underneath the
  // placeholder's parent.
  nsIFrame *placeholder =
    aPresContext->FrameManager()->GetPlaceholderFrameFor(this);
  if (!placeholder) {
    NS_NOTREACHED("no placeholder frame for out-of-flow frame");
    GetCorrectedParent(aPresContext, this, aProviderFrame);
    return NS_ERROR_FAILURE;
  }
  return NS_STATIC_CAST(nsFrame*, placeholder)->
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
    child = child->GetFirstChild(nsnull);
    if (!child)
      return;//nothing to do
    nsIFrame* siblingFrame;
    nsIContent* content;
    //ignore anonymous elements, e.g. mozTableAdd* mozTableRemove*
    //see bug 278197 comment #12 #13 for details
    while ((siblingFrame = child->GetNextSibling()) &&
           (content = siblingFrame->GetContent()) &&
           !content->IsNativeAnonymous())
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
    child = child->GetFirstChild(nsnull);
    if (!child)
      return;//nothing to do
    *aFrame = child;
  }
}

NS_IMETHODIMP
nsFrame::CaptureMouse(nsPresContext* aPresContext, PRBool aGrabMouseEvents)
{
  // get its view
  nsIView* view = GetNearestCapturingView(this);
  if (!view) {
    return NS_ERROR_FAILURE;
  }

  nsIViewManager* viewMan = view->GetViewManager();
  if (!viewMan) {
    return NS_ERROR_FAILURE;
  }

  if (aGrabMouseEvents) {
    PRBool result;
    viewMan->GrabMouseEvents(view, result);
  } else {
    PRBool result;
    viewMan->GrabMouseEvents(nsnull, result);
  }

  return NS_OK;
}

PRBool
nsFrame::IsMouseCaptured(nsPresContext* aPresContext)
{
    // get its view
  nsIView* view = GetNearestCapturingView(this);
  
  if (view) {
    nsIViewManager* viewMan = view->GetViewManager();

    if (viewMan) {
        nsIView* grabbingView;
        viewMan->GetMouseEventGrabber(grabbingView);
        if (grabbingView == view)
          return PR_TRUE;
    }
  }

  return PR_FALSE;
}

nsresult
nsIFrame::SetProperty(nsIAtom*           aPropName,
                      void*              aPropValue,
                      NSPropertyDtorFunc aPropDtorFunc,
                      void*              aDtorData)
{
  return GetPresContext()->PropertyTable()->
    SetProperty(this, aPropName, aPropValue, aPropDtorFunc, aDtorData);
}

void* 
nsIFrame::GetProperty(nsIAtom* aPropName, nsresult* aStatus) const
{
  return GetPresContext()->PropertyTable()->GetProperty(this, aPropName,
                                                        aStatus);
}

/* virtual */ void* 
nsIFrame::GetPropertyExternal(nsIAtom* aPropName, nsresult* aStatus) const
{
  return GetProperty(aPropName, aStatus);
}

nsresult
nsIFrame::DeleteProperty(nsIAtom* aPropName) const
{
  return GetPresContext()->PropertyTable()->DeleteProperty(this, aPropName);
}

void*
nsIFrame::UnsetProperty(nsIAtom* aPropName, nsresult* aStatus) const
{
  return GetPresContext()->PropertyTable()->UnsetProperty(this, aPropName,
                                                          aStatus);
}

/* virtual */ const nsStyleStruct*
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

  if (mContent && mContent->IsNodeOfType(nsINode::eELEMENT) &&
      AreAncestorViewsVisible()) {
    const nsStyleVisibility* vis = GetStyleVisibility();
    if (vis->mVisible != NS_STYLE_VISIBILITY_COLLAPSE &&
        vis->mVisible != NS_STYLE_VISIBILITY_HIDDEN) {
      if (mContent->IsNodeOfType(nsINode::eHTML)) {
        nsCOMPtr<nsISupports> container(GetPresContext()->GetContainer());
        nsCOMPtr<nsIEditorDocShell> editorDocShell(do_QueryInterface(container));
        if (editorDocShell) {
          PRBool isEditable;
          editorDocShell->GetEditable(&isEditable);
          if (isEditable) {
            return NS_OK;  // Editor content is not focusable
          }
        }
      }
      const nsStyleUserInterface* ui = GetStyleUserInterface();
      if (ui->mUserFocus != NS_STYLE_USER_FOCUS_IGNORE &&
          ui->mUserFocus != NS_STYLE_USER_FOCUS_NONE) {
        // Pass in default tabindex of -1 for nonfocusable and 0 for focusable
        tabIndex = 0;
      }
      isFocusable = mContent->IsFocusable(&tabIndex);
      if (!isFocusable && !aWithMouse &&
          GetType() == nsGkAtoms::scrollFrame &&
          mContent->IsNodeOfType(nsINode::eHTML) &&
          !mContent->IsNativeAnonymous() && mContent->GetParent() &&
          !mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::tabindex)) {
        // Elements with scrollable view are focusable with script & tabbable
        // Otherwise you couldn't scroll them with keyboard, which is
        // an accessibility issue (e.g. Section 508 rules)
        // However, we don't make them to be focusable with the mouse,
        // because the extra focus outlines are considered unnecessarily ugly.
        // When clicked on, the selection position within the element 
        // will be enough to make them keyboard scrollable.
        nsCOMPtr<nsIScrollableFrame> scrollFrame = do_QueryInterface(this);
        if (scrollFrame) {
          nsIScrollableFrame::ScrollbarStyles styles =
            scrollFrame->GetScrollbarStyles();
          if (styles.mVertical == NS_STYLE_OVERFLOW_SCROLL ||
              styles.mVertical == NS_STYLE_OVERFLOW_AUTO ||
              styles.mHorizontal == NS_STYLE_OVERFLOW_SCROLL ||
              styles.mHorizontal == NS_STYLE_OVERFLOW_AUTO) {
            // Scroll bars will be used for overflow
            isFocusable = PR_TRUE;
            tabIndex = 0;
          }
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
    nsresult rv = item->mImage->GetImageStatus(&status);
    if (NS_SUCCEEDED(rv) && (status & imgIRequest::STATUS_FRAME_COMPLETE)) {
      // This is the one we want
      item->mImage->GetImage(getter_AddRefs(aCursor.mContainer));
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
  nsIRenderingContext* rendContext = aState.GetRenderingContext();
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
    nsCOMPtr<nsILineIterator> lines = do_QueryInterface(NS_STATIC_CAST(nsIFrame*, this));
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

nsSize
nsFrame::GetPrefSize(nsBoxLayoutState& aState)
{
  nsSize size(0,0);
  DISPLAY_PREF_SIZE(this, size);
  // If the size is cached, and there are no HTML constraints that we might
  // be depending on, then we just return the cached size.
  nsBoxLayoutMetrics *metrics = BoxMetrics();
  if (!DoesNeedRecalc(metrics->mPrefSize)) {
    size = metrics->mPrefSize;
    return size;
  }

  if (IsCollapsed(aState))
    return size;

  // get our size in CSS.
  PRBool completelyRedefined = nsIBox::AddCSSPrefSize(aState, this, size);

  // Refresh our caches with new sizes.
  if (!completelyRedefined) {
    RefreshSizeCache(aState);
    size = metrics->mBlockPrefSize;

    // notice we don't need to add our borders or padding
    // in. That's because the block did it for us.
    // but we do need to add insets so debugging will work.
    AddInset(size);
    nsIBox::AddCSSPrefSize(aState, this, size);
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
  PRBool completelyRedefined = nsIBox::AddCSSMinSize(aState, this, size);

  // Refresh our caches with new sizes.
  if (!completelyRedefined) {
    RefreshSizeCache(aState);
    size = metrics->mBlockMinSize;
    AddInset(size);
    nsIBox::AddCSSMinSize(aState, this, size);
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
    nsMargin m(0, 0, 0, 0);
    GetInset(m);
    metrics->mAscent += m.top;
  }

  return metrics->mAscent;
}

nsresult
nsFrame::DoLayout(nsBoxLayoutState& aState)
{
  nsRect ourRect(mRect);

  nsIRenderingContext* rendContext = aState.GetRenderingContext();
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

  SyncLayout(aState);

  return rv;
}

nsresult
nsFrame::BoxReflow(nsBoxLayoutState&        aState,
                   nsPresContext*           aPresContext,
                   nsHTMLReflowMetrics&     aDesiredSize,
                   nsIRenderingContext*     aRenderingContext,
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

  PRBool needsReflow =
    (GetStateBits() & (NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN)) != 0;

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
    parentFrame->RemoveStateBits(0xffffffff);
    parentFrame->AddStateBits(savedState);

    // This may not do very much useful, but it's probably worth trying.
    if (parentSize.width != NS_INTRINSICSIZE)
      parentReflowState.SetComputedWidth(parentSize.width);
    if (parentSize.height != NS_INTRINSICSIZE)
      parentReflowState.mComputedHeight = parentSize.height;
    parentReflowState.mComputedMargin.SizeTo(0, 0, 0, 0);
    // XXX use box methods
    parentFrame->GetPadding(parentReflowState.mComputedPadding);
    parentFrame->GetBorder(parentReflowState.mComputedBorderPadding);
    parentReflowState.mComputedBorderPadding +=
      parentReflowState.mComputedPadding;

    // XXX Is it OK that this reflow state has no parent reflow state?
    // (It used to have a bogus parent, skipping all the boxes).
    nsHTMLReflowState reflowState(aPresContext, this, aRenderingContext,
                                  nsSize(aWidth, NS_INTRINSICSIZE));

    // Construct the parent chain manually since constructing it normally
    // messes up dimensions.
    reflowState.parentReflowState = &parentReflowState;
    reflowState.mCBReflowState = &parentReflowState;

    // mComputedWidth and mComputedHeight are content-box, not
    // border-box
    if (aWidth != NS_INTRINSICSIZE) {
      nscoord computedWidth =
        aWidth - reflowState.mComputedBorderPadding.LeftRight();
      computedWidth = PR_MAX(computedWidth, 0);
      reflowState.SetComputedWidth(computedWidth);
    }
    if (aHeight != NS_INTRINSICSIZE) {
      reflowState.mComputedHeight =
        aHeight - reflowState.mComputedBorderPadding.TopBottom();
      if (reflowState.mComputedHeight < 0)
        reflowState.mComputedHeight = 0;
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
      printf("Size=(%d,%d)\n",reflowState.ComputedWidth(), reflowState.mComputedHeight);
      nsAdaptorAddIndents();
      nsAdaptorPrintReason(reflowState);
      printf("\n");
    #endif

       // place the child and reflow
    WillReflow(aPresContext);

    Reflow(aPresContext, aDesiredSize, reflowState, status);

    NS_ASSERTION(NS_FRAME_IS_COMPLETE(status), "bad status");

   // printf("width: %d, height: %d\n", aDesiredSize.mCombinedArea.width, aDesiredSize.mCombinedArea.height);

    // see if the overflow option is set. If it is then if our child's bounds overflow then
    // we will set the child's rect to include the overflow size.
    if (GetStateBits() & NS_FRAME_OUTSIDE_CHILDREN) {
      // make sure we store the overflow size
      
      // This kinda sucks. We should be able to handle the case
      // where there's overflow above or to the left of the
      // origin. But for now just chop that stuff off.

      //printf("OutsideChildren width=%d, height=%d\n", aDesiredSize.mOverflowArea.width, aDesiredSize.mOverflowArea.height);
      aDesiredSize.width = aDesiredSize.mOverflowArea.XMost();
      if (aDesiredSize.width <= aWidth)
        aDesiredSize.height = aDesiredSize.mOverflowArea.YMost();
      else {
        if (aDesiredSize.width > aWidth)
        {
          nscoord computedWidth = aDesiredSize.width -
            reflowState.mComputedBorderPadding.LeftRight();
          computedWidth = PR_MAX(computedWidth, 0);
          reflowState.SetComputedWidth(computedWidth);
          reflowState.availableWidth = aDesiredSize.width;
          DidReflow(aPresContext, &reflowState, NS_FRAME_REFLOW_FINISHED);
          #ifdef DEBUG_REFLOW
           nsAdaptorAddIndents();
           nsAdaptorPrintReason(reflowState);
           printf("\n");
          #endif
          AddStateBits(NS_FRAME_IS_DIRTY);
          WillReflow(aPresContext);
          Reflow(aPresContext, aDesiredSize, reflowState, status);
          if (GetStateBits() & NS_FRAME_OUTSIDE_CHILDREN)
            aDesiredSize.height = aDesiredSize.mOverflowArea.YMost();

        }
      }
    }

    if (redrawAfterReflow) {
       nsRect r = GetRect();
       r.width = aDesiredSize.width;
       r.height = aDesiredSize.height;
       Redraw(aState, &r);
    }

    PRBool changedSize = PR_FALSE;

    if (metrics->mLastSize.width != aDesiredSize.width || metrics->mLastSize.height != aDesiredSize.height)
       changedSize = PR_TRUE;
  
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

PRBool
nsFrame::GetWasCollapsed(nsBoxLayoutState& aState)
{
  return BoxMetrics()->mWasCollapsed;
}

void
nsFrame::SetWasCollapsed(nsBoxLayoutState& aState, PRBool aCollapsed)
{
  BoxMetrics()->mWasCollapsed = aCollapsed;
}

nsBoxLayoutMetrics*
nsFrame::BoxMetrics() const
{
  nsBoxLayoutMetrics* metrics =
    NS_STATIC_CAST(nsBoxLayoutMetrics*, GetProperty(nsGkAtoms::boxMetricsProperty));
  NS_ASSERTION(metrics, "A box layout method was called but InitBoxMetrics was never called");
  return metrics;
}

NS_IMETHODIMP
nsFrame::SetParent(const nsIFrame* aParent)
{
  PRBool wasBoxWrapped = IsBoxWrapped();
  nsIFrame::SetParent(aParent);
  if (!wasBoxWrapped && IsBoxWrapped())
    InitBoxMetrics(PR_TRUE);
  else if (wasBoxWrapped && !IsBoxWrapped())
    DeleteProperty(nsGkAtoms::boxMetricsProperty);

  if (aParent && aParent->IsBoxFrame()) {
    if (aParent->ChildrenMustHaveWidgets()) {
        nsHTMLContainerFrame::CreateViewForFrame(this, nsnull, PR_TRUE);
        nsIView* view = GetView();
        if (!view->HasWidget())
          CreateWidgetForView(view);
    }
  }

  return NS_OK;
}

static void
DeleteBoxMetrics(void    *aObject,
                 nsIAtom *aPropertyName,
                 void    *aPropertyValue,
                 void    *aData)
{
  delete NS_STATIC_CAST(nsBoxLayoutMetrics*, aPropertyValue);
}

void
nsFrame::InitBoxMetrics(PRBool aClear)
{
  if (aClear)
    DeleteProperty(nsGkAtoms::boxMetricsProperty);

  nsBoxLayoutMetrics *metrics = new nsBoxLayoutMetrics();
  SetProperty(nsGkAtoms::boxMetricsProperty, metrics, DeleteBoxMetrics);

  nsFrame::MarkIntrinsicWidthsDirty();
  metrics->mBlockAscent = 0;
  metrics->mLastSize.SizeTo(0, 0);
  metrics->mWasCollapsed = PR_FALSE;
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
   nsIFrameDebug*  frameDebug;
   nsAutoString name;
   if (NS_SUCCEEDED(QueryInterface(NS_GET_IID(nsIFrameDebug), (void**)&frameDebug))) {
      frameDebug->GetFrameName(name);
   }

  aName = name;
}
#endif

#ifdef NS_DEBUG
static void
GetTagName(nsFrame* aFrame, nsIContent* aContent, PRIntn aResultSize,
           char* aResult)
{
  const char *nameStr = "";
  if (aContent) {
    aContent->Tag()->GetUTF8String(&nameStr);
  }
  PR_snprintf(aResult, aResultSize, "%s@%p", nameStr, aFrame);
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
nsFrame::VerifyDirtyBitSet(nsIFrame* aFrameList)
{
  for (nsIFrame*f = aFrameList; f; f = f->GetNextSibling()) {
    NS_ASSERTION(f->GetStateBits() & NS_FRAME_IS_DIRTY, "dirty bit not set");
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

MOZ_DECL_CTOR_COUNTER(DR_layout_cookie)

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

MOZ_DECL_CTOR_COUNTER(DR_intrinsic_width_cookie)

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

MOZ_DECL_CTOR_COUNTER(DR_intrinsic_size_cookie)

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
                  char* aBuf);
  DR_Rule* ParseRule(FILE* aFile);
  void ParseRulesFile();
  void AddRule(nsVoidArray& aRules,
               DR_Rule&     aRule);
  PRBool IsWhiteSpace(int c);
  PRBool GetNumber(char*    aBuf, 
                 PRInt32&  aNumber);
  void PrettyUC(nscoord aSize,
                char*   aBuf);
  void DisplayFrameTypeInfo(nsIFrame* aFrame,
                            PRInt32   aIndent);
  void DeleteTreeNode(DR_FrameTreeNode& aNode);

  PRBool      mInited;
  PRBool      mActive;
  PRInt32     mCount;
  nsVoidArray mWildRules;
  PRInt32     mAssert;
  PRInt32     mIndent;
  PRBool      mIndentUndisplayedFrames;
  nsVoidArray mFrameTypeTable;
  PRBool      mDisplayPixelErrors;

  // reflow specific state
  nsVoidArray mFrameTreeLeaves;
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
      MOZ_COUNT_DTOR(DR_FrameTypeInfo);
      PRInt32 numElements;
      numElements = mRules.Count();
      for (PRInt32 i = numElements - 1; i >= 0; i--) {
        delete (DR_Rule *)mRules.ElementAt(i);
      }
   }

  nsIAtom*    mType;
  char        mNameAbbrev[16];
  char        mName[32];
  nsVoidArray mRules;
};

DR_FrameTypeInfo::DR_FrameTypeInfo(nsIAtom* aFrameType, 
                                   const char* aFrameNameAbbrev, 
                                   const char* aFrameName)
{
  mType = aFrameType;
  strcpy(mNameAbbrev, aFrameNameAbbrev);
  strcpy(mName, aFrameName);
  MOZ_COUNT_CTOR(DR_FrameTypeInfo);
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
  numElements = mWildRules.Count();
  for (i = numElements - 1; i >= 0; i--) {
    delete (DR_Rule *)mWildRules.ElementAt(i);
  }
  numElements = mFrameTreeLeaves.Count();
  for (i = numElements - 1; i >= 0; i--) {
    delete (DR_FrameTreeNode *)mFrameTreeLeaves.ElementAt(i);
  }
  numElements = mFrameTypeTable.Count();
  for (i = numElements - 1; i >= 0; i--) {
    delete (DR_FrameTypeInfo *)mFrameTypeTable.ElementAt(i);
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
                          char* aBuf)
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
    PRInt32 cX;
    for (cX = 1, c = getc(aFile); ; cX++, c = getc(aFile)) {
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
  while (GetToken(aFile, buf)) {
    if (GetNumber(buf, doDisplay)) {
      if (rule) { 
        rule->mDisplay = (PRBool)doDisplay;
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

void DR_State::AddRule(nsVoidArray& aRules,
                       DR_Rule&     aRule)
{
  PRInt32 numRules = aRules.Count();
  for (PRInt32 ruleX = 0; ruleX < numRules; ruleX++) {
    DR_Rule* rule = (DR_Rule*)aRules.ElementAt(ruleX);
    NS_ASSERTION(rule, "program error");
    if (aRule.mLength > rule->mLength) {
      aRules.InsertElementAt(&aRule, ruleX);
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
  mFrameTypeTable.AppendElement(new DR_FrameTypeInfo(aFrameType, aFrameNameAbbrev, aFrameName));
}

DR_FrameTypeInfo* DR_State::GetFrameTypeInfo(nsIAtom* aFrameType)
{
  PRInt32 numEntries = mFrameTypeTable.Count();
  NS_ASSERTION(numEntries != 0, "empty FrameTypeTable");
  for (PRInt32 i = 0; i < numEntries; i++) {
    DR_FrameTypeInfo* info = (DR_FrameTypeInfo*)mFrameTypeTable.ElementAt(i);
    if (info && (info->mType == aFrameType)) {
      return info;
    }
  }
  return (DR_FrameTypeInfo*)mFrameTypeTable.ElementAt(numEntries - 1); // return unknown frame type
}

DR_FrameTypeInfo* DR_State::GetFrameTypeInfo(char* aFrameName)
{
  PRInt32 numEntries = mFrameTypeTable.Count();
  NS_ASSERTION(numEntries != 0, "empty FrameTypeTable");
  for (PRInt32 i = 0; i < numEntries; i++) {
    DR_FrameTypeInfo* info = (DR_FrameTypeInfo*)mFrameTypeTable.ElementAt(i);
    if (info && ((strcmp(aFrameName, info->mName) == 0) || (strcmp(aFrameName, info->mNameAbbrev) == 0))) {
      return info;
    }
  }
  return (DR_FrameTypeInfo*)mFrameTypeTable.ElementAt(numEntries - 1); // return unknown frame type
}

void DR_State::InitFrameTypeTable()
{  
  AddFrameTypeInfo(nsGkAtoms::areaFrame,             "area",      "area");
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
      nsAutoString  name;
      nsIFrameDebug*  frameDebug;
      if (NS_SUCCEEDED(aFrame->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**)&frameDebug))) {
       frameDebug->GetFrameName(name);
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
  PRInt32 numRules = info->mRules.Count();
  for (PRInt32 ruleX = 0; ruleX < numRules; ruleX++) {
    DR_Rule* rule = (DR_Rule*)info->mRules.ElementAt(ruleX);
    if (rule && RuleMatches(*rule, aNode)) {
      aNode.mDisplay = rule->mDisplay;
      matchingRule = PR_TRUE;
      break;
    }
  }
  if (!matchingRule) {
    PRInt32 numWildRules = mWildRules.Count();
    for (PRInt32 ruleX = 0; ruleX < numWildRules; ruleX++) {
      DR_Rule* rule = (DR_Rule*)mWildRules.ElementAt(ruleX);
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
  if(mFrameTreeLeaves.Count())
    lastLeaf = (DR_FrameTreeNode*)mFrameTreeLeaves.ElementAt(mFrameTreeLeaves.Count() - 1);
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
    mFrameTreeLeaves.RemoveElementAt(mFrameTreeLeaves.Count() - 1);
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

void DR_State::DeleteTreeNode(DR_FrameTreeNode& aNode)
{
  mFrameTreeLeaves.RemoveElement(&aNode);
  PRInt32 numLeaves = mFrameTreeLeaves.Count();
  if ((0 == numLeaves) || (aNode.mParent != (DR_FrameTreeNode*)mFrameTreeLeaves.ElementAt(numLeaves - 1))) {
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
    DR_state->PrettyUC(aReflowState.mComputedHeight, height);
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
      CheckPixelError(aReflowState.mComputedHeight, p2t);
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
    printf("Reflow d=%s,%s ", width, height);

    if (NS_FRAME_IS_NOT_COMPLETE(aStatus)) {
      printf("status=0x%x", aStatus);
    }
    if (aFrame->GetStateBits() & NS_FRAME_OUTSIDE_CHILDREN) {
       DR_state->PrettyUC(aMetrics.mOverflowArea.x, x);
       DR_state->PrettyUC(aMetrics.mOverflowArea.y, y);
       DR_state->PrettyUC(aMetrics.mOverflowArea.width, width);
       DR_state->PrettyUC(aMetrics.mOverflowArea.height, height);
       printf("o=(%s,%s) %s x %s", x, y, width, height);
       nsRect* storedOverflow = aFrame->GetOverflowAreaProperty();
       if (storedOverflow) {
         if (aMetrics.mOverflowArea != *storedOverflow) {
           DR_state->PrettyUC(storedOverflow->x, x);
           DR_state->PrettyUC(storedOverflow->y, y);
           DR_state->PrettyUC(storedOverflow->width, width);
           DR_state->PrettyUC(storedOverflow->height, height);
           printf("sto=(%s,%s) %s x %s", x, y, width, height);
         }
       }
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
    printf("Get%sWidth=%d\n", aType, aResult);
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

#endif
// End Display Reflow

#endif
