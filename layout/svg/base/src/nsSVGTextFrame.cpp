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
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
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

#include "nsIDOMSVGTextElement.h"
#include "nsPresContext.h"
#include "nsISVGTextFrame.h"
#include "nsISVGRendererCanvas.h"
#include "nsWeakReference.h"
#include "nsISVGValue.h"
#include "nsISVGValueObserver.h"
#include "nsIDOMSVGSVGElement.h"
#include "nsIDOMSVGMatrix.h"
#include "nsIDOMSVGLengthList.h"
#include "nsIDOMSVGLength.h"
#include "nsISVGValueUtils.h"
#include "nsIDOMSVGAnimatedLengthList.h"
#include "nsSVGContainerFrame.h"
#include "nsISVGChildFrame.h"
#include "nsISVGGlyphFragmentNode.h"
#include "nsISVGGlyphFragmentLeaf.h"
#include "nsISVGRendererGlyphMetrics.h"
#include "nsSVGOuterSVGFrame.h"
#include "nsIDOMSVGRect.h"
#include "nsISVGTextContentMetrics.h"
#include "nsSVGRect.h"
#include "nsSVGMatrix.h"
#include "nsLayoutAtoms.h"
#include "nsSVGTextPathFrame.h"
#include "nsSVGPathElement.h"
#include "nsSVGUtils.h"
#include "nsSVGUtils.h"
#include "nsSVGClipPathFrame.h"
#include "nsISVGRendererSurface.h"
#include "nsINameSpaceManager.h"
#include "nsGkAtoms.h"
#include "nsSVGGraphicElement.h"

typedef nsSVGDisplayContainerFrame nsSVGTextFrameBase;

class nsSVGTextFrame : public nsSVGTextFrameBase,
                       public nsISVGTextFrame, // : nsISVGTextContainerFrame
                       public nsISVGValueObserver,
                       public nsISVGTextContentMetrics,
                       public nsSupportsWeakReference
{
  friend nsIFrame*
  NS_NewSVGTextFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext);
protected:
  nsSVGTextFrame(nsStyleContext* aContext);

   // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }  
public:
  // nsIFrame:
  NS_IMETHOD  RemoveFrame(nsIAtom*        aListName,
                          nsIFrame*       aOldFrame);
  
  NS_IMETHOD  AttributeChanged(PRInt32         aNameSpaceID,
                               nsIAtom*        aAttribute,
                               PRInt32         aModType);

  NS_IMETHOD DidSetStyleContext();

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::svgTextFrame
   */
  virtual nsIAtom* GetType() const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGText"), aResult);
  }
#endif

  // nsISVGValueObserver
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable,
                                     nsISVGValue::modificationType aModType);
  NS_IMETHOD DidModifySVGObservable (nsISVGValue* observable,
                                     nsISVGValue::modificationType aModType);

  // nsISVGTextContentMetrics
  NS_IMETHOD GetNumberOfChars(PRInt32 *_retval);
  NS_IMETHOD GetComputedTextLength(float *_retval);
  NS_IMETHOD GetSubStringLength(PRUint32 charnum, PRUint32 nchars, float *_retval);
  NS_IMETHOD GetStartPositionOfChar(PRUint32 charnum, nsIDOMSVGPoint **_retval);
  NS_IMETHOD GetEndPositionOfChar(PRUint32 charnum, nsIDOMSVGPoint **_retval);
  NS_IMETHOD GetExtentOfChar(PRUint32 charnum, nsIDOMSVGRect **_retval);
  NS_IMETHOD GetRotationOfChar(PRUint32 charnum, float *_retval);
  NS_IMETHOD GetCharNumAtPosition(nsIDOMSVGPoint *point, PRInt32 *_retval);
  
  // nsISupportsWeakReference
  // implementation inherited from nsSupportsWeakReference
  
  // nsISVGChildFrame interface:
  NS_IMETHOD PaintSVG(nsISVGRendererCanvas* canvas);
  NS_IMETHOD NotifyCanvasTMChanged(PRBool suppressInvalidation);
  NS_IMETHOD NotifyRedrawSuspended();
  NS_IMETHOD NotifyRedrawUnsuspended();
  NS_IMETHOD SetMatrixPropagation(PRBool aPropagate);
  NS_IMETHOD SetOverrideCTM(nsIDOMSVGMatrix *aCTM);
  NS_IMETHOD GetBBox(nsIDOMSVGRect **_retval);
  
  // nsSVGContainerFrame methods:
  virtual already_AddRefed<nsIDOMSVGMatrix> GetCanvasTM();
  
  // nsISVGTextFrame interface:
  NS_IMETHOD_(void) NotifyGlyphMetricsChange(nsISVGGlyphFragmentNode* caller);
  NS_IMETHOD_(void) NotifyGlyphFragmentTreeChange(nsISVGGlyphFragmentNode* caller);
  NS_IMETHOD_(PRBool) IsMetricsSuspended();
  NS_IMETHOD_(PRBool) IsGlyphFragmentTreeSuspended();

  // nsISVGTextContainerFrame interface:
  NS_IMETHOD_(nsISVGTextFrame *) GetTextFrame();
  NS_IMETHOD_(PRBool) GetAbsolutePositionAdjustmentX(float &x, PRUint32 charNum);
  NS_IMETHOD_(PRBool) GetAbsolutePositionAdjustmentY(float &y, PRUint32 charNum);
  NS_IMETHOD_(PRBool) GetRelativePositionAdjustmentX(float &dx, PRUint32 charNum);
  NS_IMETHOD_(PRBool) GetRelativePositionAdjustmentY(float &dy, PRUint32 charNum);
  NS_IMETHOD_(already_AddRefed<nsIDOMSVGLengthList>) GetX();
  NS_IMETHOD_(already_AddRefed<nsIDOMSVGLengthList>) GetY();
  NS_IMETHOD_(already_AddRefed<nsIDOMSVGLengthList>) GetDx();
  NS_IMETHOD_(already_AddRefed<nsIDOMSVGLengthList>) GetDy();

protected:
  void EnsureFragmentTreeUpToDate();
  void UpdateFragmentTree();
  void UpdateGlyphPositioning();

  enum UpdateState{
    unsuspended,
    suspended,
    updating};
  UpdateState mFragmentTreeState;
  UpdateState mMetricsState;

  nsCOMPtr<nsIDOMSVGMatrix> mCanvasTM;
  nsCOMPtr<nsIDOMSVGMatrix> mOverrideCTM;

  PRPackedBool mFragmentTreeDirty;
  PRPackedBool mPositioningDirty;

  PRPackedBool mPropagateTransform;
};

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGTextFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext)
{
  nsCOMPtr<nsIDOMSVGTextElement> text_elem = do_QueryInterface(aContent);
  if (!text_elem) {
#ifdef DEBUG
    printf("warning: trying to construct an SVGTextFrame for a "
           "content element that doesn't support the right interfaces\n");
#endif
    return nsnull;
  }

  return new (aPresShell) nsSVGTextFrame(aContext);
}

nsSVGTextFrame::nsSVGTextFrame(nsStyleContext* aContext)
    : nsSVGTextFrameBase(aContext),
      mFragmentTreeState(suspended), mMetricsState(suspended),
      mFragmentTreeDirty(PR_FALSE), mPositioningDirty(PR_FALSE),
      mPropagateTransform(PR_TRUE)
{
}

//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN(nsSVGTextFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGTextFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGTextContainerFrame)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
  NS_INTERFACE_MAP_ENTRY(nsISVGTextContentMetrics)
NS_INTERFACE_MAP_END_INHERITING(nsSVGTextFrameBase)


//----------------------------------------------------------------------
// nsIFrame methods

NS_IMETHODIMP
nsSVGTextFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                 nsIAtom*        aAttribute,
                                 PRInt32         aModType)
{
  if (aNameSpaceID != kNameSpaceID_None)
    return NS_OK;

  if (aAttribute == nsGkAtoms::transform) {
    // transform has changed

    // make sure our cached transform matrix gets (lazily) updated
    mCanvasTM = nsnull;
    
    nsIFrame* kid = mFrames.FirstChild();
    while (kid) {
      nsISVGChildFrame* SVGFrame=0;
      kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
      if (SVGFrame)
        SVGFrame->NotifyCanvasTMChanged(PR_FALSE);
      kid = kid->GetNextSibling();
    }
  } else if (aAttribute == nsGkAtoms::x ||
             aAttribute == nsGkAtoms::y ||
             aAttribute == nsGkAtoms::dx ||
             aAttribute == nsGkAtoms::dy) {
    mPositioningDirty = PR_TRUE;
    if (mMetricsState == unsuspended) {
      UpdateGlyphPositioning();
    }
  }

 return NS_OK;
}

NS_IMETHODIMP
nsSVGTextFrame::DidSetStyleContext()
{
#ifdef DEBUG
  printf("** nsSVGTextFrame::DidSetStyleContext\n");
#endif
  nsSVGUtils::StyleEffects(this);

  return NS_OK;
}

nsIAtom *
nsSVGTextFrame::GetType() const
{
  return nsLayoutAtoms::svgTextFrame;
}

NS_IMETHODIMP
nsSVGTextFrame::RemoveFrame(nsIAtom*        aListName,
                            nsIFrame*       aOldFrame)
{
  nsSVGOuterSVGFrame* outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(this);
  if (outerSVGFrame)
    outerSVGFrame->SuspendRedraw();
  mFragmentTreeDirty = PR_TRUE;

  nsresult rv = nsSVGTextFrameBase::RemoveFrame(aListName, aOldFrame);
  
  if (outerSVGFrame)
    outerSVGFrame->UnsuspendRedraw();

  return rv;
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods:

NS_IMETHODIMP
nsSVGTextFrame::WillModifySVGObservable(nsISVGValue* observable,
                                        nsISVGValue::modificationType aModType)
{
  nsSVGUtils::WillModifyEffects(this, observable, aModType);

  return NS_OK;
}


NS_IMETHODIMP
nsSVGTextFrame::DidModifySVGObservable (nsISVGValue* observable,
                                        nsISVGValue::modificationType aModType)
{  
  nsSVGUtils::DidModifyEffects(this, observable, aModType);

  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGTextContentMetrics
NS_IMETHODIMP
nsSVGTextFrame::GetNumberOfChars(PRInt32 *_retval)
{
  EnsureFragmentTreeUpToDate();

  *_retval = nsSVGUtils::GetNumberOfChars(&mFrames);

  return NS_OK;
}

NS_IMETHODIMP
nsSVGTextFrame::GetComputedTextLength(float *_retval)
{
  EnsureFragmentTreeUpToDate();

  *_retval = nsSVGUtils::GetComputedTextLength(&mFrames);

  return NS_OK;
}

NS_IMETHODIMP
nsSVGTextFrame::GetSubStringLength(PRUint32 charnum, PRUint32 nchars, float *_retval)
{
  EnsureFragmentTreeUpToDate();

  return nsSVGUtils::GetSubStringLength(&mFrames, charnum, nchars, _retval);
}

NS_IMETHODIMP
nsSVGTextFrame::GetStartPositionOfChar(PRUint32 charnum, nsIDOMSVGPoint **_retval)
{
  EnsureFragmentTreeUpToDate();

  return nsSVGUtils::GetStartPositionOfChar(&mFrames, charnum, _retval);
}

NS_IMETHODIMP
nsSVGTextFrame::GetEndPositionOfChar(PRUint32 charnum, nsIDOMSVGPoint **_retval)
{
  EnsureFragmentTreeUpToDate();

  return nsSVGUtils::GetEndPositionOfChar(&mFrames, charnum, _retval);
}

NS_IMETHODIMP
nsSVGTextFrame::GetExtentOfChar(PRUint32 charnum, nsIDOMSVGRect **_retval)
{
  EnsureFragmentTreeUpToDate();

  return nsSVGUtils::GetExtentOfChar(&mFrames, charnum, _retval);
}

NS_IMETHODIMP
nsSVGTextFrame::GetRotationOfChar(PRUint32 charnum, float *_retval)
{
  EnsureFragmentTreeUpToDate();

  return nsSVGUtils::GetRotationOfChar(&mFrames, charnum, _retval);
}

NS_IMETHODIMP
nsSVGTextFrame::GetCharNumAtPosition(nsIDOMSVGPoint *point, PRInt32 *_retval)
{
  EnsureFragmentTreeUpToDate();

  *_retval = nsSVGUtils::GetCharNumAtPosition(&mFrames, point);

  return NS_OK;
}


//----------------------------------------------------------------------
// nsISVGChildFrame methods

NS_IMETHODIMP
nsSVGTextFrame::PaintSVG(nsISVGRendererCanvas* canvas)
{
  const nsStyleDisplay *display = mStyleContext->GetStyleDisplay();
  if (display->mOpacity == 0.0)
    return NS_OK;

  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame* SVGFrame = nsnull;
    CallQueryInterface(kid, &SVGFrame);
    if (SVGFrame)
      SVGFrame->PaintSVG(canvas);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSVGTextFrame::NotifyCanvasTMChanged(PRBool suppressInvalidation)
{
  // make sure our cached transform matrix gets (lazily) updated
  mCanvasTM = nsnull;

  return nsSVGTextFrameBase::NotifyCanvasTMChanged(suppressInvalidation);
}

NS_IMETHODIMP
nsSVGTextFrame::NotifyRedrawSuspended()
{
  mMetricsState = suspended;
  mFragmentTreeState = suspended;

  nsSVGTextFrameBase::NotifyRedrawSuspended();

  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGGlyphFragmentNode* fragmentNode = nsnull;
    CallQueryInterface(kid, &fragmentNode);
    if (fragmentNode) {
      fragmentNode->NotifyMetricsSuspended();
      fragmentNode->NotifyGlyphFragmentTreeSuspended();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSVGTextFrame::NotifyRedrawUnsuspended()
{
  NS_ASSERTION(mMetricsState == suspended, "metrics state not suspended during redraw");
  NS_ASSERTION(mFragmentTreeState == suspended, "fragment tree not suspended during redraw");

  // 3 passes:
  nsIFrame *kid;
  mFragmentTreeState = updating;
  for (kid = mFrames.FirstChild(); kid; kid = kid->GetNextSibling()) {
    nsISVGGlyphFragmentNode* node = nsnull;
    CallQueryInterface(kid, &node);
    if (node) {
      node->NotifyGlyphFragmentTreeUnsuspended();
    }
  }

  mFragmentTreeState = unsuspended;
  if (mFragmentTreeDirty)
    UpdateFragmentTree();
  
  mMetricsState = updating;
  for (kid = mFrames.FirstChild(); kid; kid = kid->GetNextSibling()) {
    nsISVGGlyphFragmentNode* node = nsnull;
    CallQueryInterface(kid, &node);
    if (node) {
      node->NotifyMetricsUnsuspended();
    }
  }

  mMetricsState = unsuspended;
  if (mPositioningDirty)
    UpdateGlyphPositioning();
  
  nsSVGTextFrameBase::NotifyRedrawUnsuspended();
  
  return NS_OK;
}

NS_IMETHODIMP
nsSVGTextFrame::SetMatrixPropagation(PRBool aPropagate)
{
  mPropagateTransform = aPropagate;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGTextFrame::SetOverrideCTM(nsIDOMSVGMatrix *aCTM)
{
  mOverrideCTM = aCTM;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGTextFrame::GetBBox(nsIDOMSVGRect **_retval)
{
  EnsureFragmentTreeUpToDate();
  return nsSVGTextFrameBase::GetBBox(_retval);
}

//----------------------------------------------------------------------
// nsSVGContainerFrame methods:

already_AddRefed<nsIDOMSVGMatrix>
nsSVGTextFrame::GetCanvasTM()
{
  if (!mPropagateTransform) {
    nsIDOMSVGMatrix *retval;
    if (mOverrideCTM) {
      retval = mOverrideCTM;
      NS_ADDREF(retval);
    } else {
      NS_NewSVGMatrix(&retval);
    }
    return retval;
  }

  if (!mCanvasTM) {
    // get our parent's tm and append local transforms (if any):
    NS_ASSERTION(mParent, "null parent");
    nsSVGContainerFrame *containerFrame = NS_STATIC_CAST(nsSVGContainerFrame*,
                                                         mParent);
    nsCOMPtr<nsIDOMSVGMatrix> parentTM = containerFrame->GetCanvasTM();
    NS_ASSERTION(parentTM, "null TM");

    // got the parent tm, now check for local tm:
    nsSVGGraphicElement *element =
      NS_STATIC_CAST(nsSVGGraphicElement*, mContent);
    nsCOMPtr<nsIDOMSVGMatrix> localTM = element->GetLocalTransformMatrix();
    
    if (localTM)
      parentTM->Multiply(localTM, getter_AddRefs(mCanvasTM));
    else
      mCanvasTM = parentTM;
  }

  nsIDOMSVGMatrix* retval = mCanvasTM.get();
  NS_IF_ADDREF(retval);
  return retval;
}

//----------------------------------------------------------------------
// nsISVGTextFrame methods

NS_IMETHODIMP_(void)
nsSVGTextFrame::NotifyGlyphMetricsChange(nsISVGGlyphFragmentNode* caller)
{
  NS_ASSERTION(mMetricsState!=suspended, "notification during suspension");
  mPositioningDirty = PR_TRUE;
  if (mMetricsState == unsuspended) {
    UpdateGlyphPositioning();
  }
}

NS_IMETHODIMP_(void)
nsSVGTextFrame::NotifyGlyphFragmentTreeChange(nsISVGGlyphFragmentNode* caller)
{
  NS_ASSERTION(mFragmentTreeState!=suspended, "notification during suspension");
  mFragmentTreeDirty = PR_TRUE;
  if (mFragmentTreeState == unsuspended) {
    UpdateFragmentTree();
  }
}

NS_IMETHODIMP_(PRBool)
nsSVGTextFrame::IsMetricsSuspended()
{
  return (mMetricsState != unsuspended);
}

NS_IMETHODIMP_(PRBool)
nsSVGTextFrame::IsGlyphFragmentTreeSuspended()
{
  return (mFragmentTreeState != unsuspended);
}

//----------------------------------------------------------------------
// nsISVGTextContainerFrame methods:

NS_IMETHODIMP_(nsISVGTextFrame *)
nsSVGTextFrame::GetTextFrame()
{
  return this;
}

NS_IMETHODIMP_(PRBool)
nsSVGTextFrame::GetAbsolutePositionAdjustmentX(float &x, PRUint32 charNum)
{
  return PR_FALSE;
}

NS_IMETHODIMP_(PRBool)
nsSVGTextFrame::GetAbsolutePositionAdjustmentY(float &y, PRUint32 charNum)
{
  return PR_FALSE;
}

NS_IMETHODIMP_(PRBool)
nsSVGTextFrame::GetRelativePositionAdjustmentX(float &dx, PRUint32 charNum)
{
  return PR_FALSE;
}

NS_IMETHODIMP_(PRBool)
nsSVGTextFrame::GetRelativePositionAdjustmentY(float &dy, PRUint32 charNum)
{
  return PR_FALSE;  
}

//----------------------------------------------------------------------
//

// ensure that the tree and positioning of the nodes is up-to-date
void
nsSVGTextFrame::EnsureFragmentTreeUpToDate()
{
  PRBool resuspend_fragmenttree = PR_FALSE;
  PRBool resuspend_metrics = PR_FALSE;
  
  // give children a chance to flush their change notifications:
  
  if (mFragmentTreeState == suspended) {
    resuspend_fragmenttree = PR_TRUE;
    mFragmentTreeState = updating;
    nsIFrame* kid = mFrames.FirstChild();
    while (kid) {
      nsISVGGlyphFragmentNode* node=nsnull;
      kid->QueryInterface(NS_GET_IID(nsISVGGlyphFragmentNode), (void**)&node);
      if (node)
        node->NotifyGlyphFragmentTreeUnsuspended();
      kid = kid->GetNextSibling();
    }
    
    mFragmentTreeState = unsuspended;
  }

  if (mFragmentTreeDirty)
    UpdateFragmentTree();

  if (mMetricsState == suspended) {
    resuspend_metrics = PR_TRUE;
    mMetricsState = updating;
    nsIFrame* kid = mFrames.FirstChild();
    while (kid) {
      nsISVGGlyphFragmentNode* node=nsnull;
      kid->QueryInterface(NS_GET_IID(nsISVGGlyphFragmentNode), (void**)&node);
      if (node)
        node->NotifyMetricsUnsuspended();
      kid = kid->GetNextSibling();
    }

    mMetricsState = unsuspended;
  }
  
  if (mPositioningDirty)
    UpdateGlyphPositioning();

  if (resuspend_fragmenttree || resuspend_metrics) {
    mMetricsState = suspended;
    mFragmentTreeState = suspended;
  
    nsIFrame* kid = mFrames.FirstChild();
    while (kid) {
      nsISVGGlyphFragmentNode* fragmentNode=nsnull;
      kid->QueryInterface(NS_GET_IID(nsISVGGlyphFragmentNode), (void**)&fragmentNode);
      if (fragmentNode) {
        fragmentNode->NotifyMetricsSuspended();
        fragmentNode->NotifyGlyphFragmentTreeSuspended();
      }
      kid = kid->GetNextSibling();
    }
  } 
}

void
nsSVGTextFrame::UpdateFragmentTree()
{
  NS_ASSERTION(mFragmentTreeState == unsuspended, "updating during suspension!");

  nsSVGUtils::BuildGlyphFragmentTree(&mFrames, 0, PR_TRUE);

  mFragmentTreeDirty = PR_FALSE;
  
  mPositioningDirty = PR_TRUE;
  if (mMetricsState == unsuspended)
    UpdateGlyphPositioning();
}

static void
GetSingleValue(nsISVGGlyphFragmentLeaf *fragment,
               nsIDOMSVGLengthList *list, float *val)
{
  if (!list)
    return;

  PRUint32 count = 0;
  list->GetNumberOfItems(&count);
#ifdef DEBUG
  if (count > 1)
    NS_WARNING("multiple lengths for x/y attributes on <text> elements not implemented yet!");
#endif
  if (count) {
    nsCOMPtr<nsIDOMSVGLength> length;
    list->GetItem(0, getter_AddRefs(length));
    length->GetValue(val);

    /* check for % sizing of textpath */
    PRUint16 type;
    length->GetUnitType(&type);
    if (type == nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE) {
      nsIFrame *glyph;
      CallQueryInterface(fragment, &glyph);

      nsSVGTextPathFrame *textPath = nsnull;
      /* check if we're the child of a textPath */
      for (nsIFrame *frame = glyph; frame != nsnull; frame = frame->GetParent())
        if (frame->GetType() == nsLayoutAtoms::svgTextPathFrame) {
          textPath = NS_STATIC_CAST(nsSVGTextPathFrame*, frame);
          break;
        }

      if (textPath) {
        nsAutoPtr<nsSVGFlattenedPath> data(textPath->GetFlattenedPath());

        if (!data)
          return;

        float percent;
        length->GetValueInSpecifiedUnits(&percent);

        *val = data->GetLength()*percent/100.0f;
      }
    }
  }
}

void
nsSVGTextFrame::UpdateGlyphPositioning()
{
  NS_ASSERTION(mMetricsState == unsuspended, "updating during suspension");

  nsISVGGlyphFragmentNode* node;
  node = nsSVGUtils::GetFirstGlyphFragmentChildNode(&mFrames);
  if (!node) return;

  // we'll align every fragment in this chunk on the dominant-baseline:
  // XXX should actually inspect 'alignment-baseline' for each fragment
  
  PRUint8 baseline;
  switch(GetStyleSVGReset()->mDominantBaseline) {
    case NS_STYLE_DOMINANT_BASELINE_TEXT_BEFORE_EDGE:
      baseline = nsISVGRendererGlyphMetrics::BASELINE_TEXT_BEFORE_EDGE;
      break;
    case NS_STYLE_DOMINANT_BASELINE_TEXT_AFTER_EDGE:
      baseline = nsISVGRendererGlyphMetrics::BASELINE_TEXT_AFTER_EDGE;
      break;
    case NS_STYLE_DOMINANT_BASELINE_MIDDLE:
      baseline = nsISVGRendererGlyphMetrics::BASELINE_MIDDLE;
      break;
    case NS_STYLE_DOMINANT_BASELINE_CENTRAL:
      baseline = nsISVGRendererGlyphMetrics::BASELINE_CENTRAL;
      break;
    case NS_STYLE_DOMINANT_BASELINE_MATHEMATICAL:
      baseline = nsISVGRendererGlyphMetrics::BASELINE_MATHEMATICAL;
      break;
    case NS_STYLE_DOMINANT_BASELINE_IDEOGRAPHIC:
      baseline = nsISVGRendererGlyphMetrics::BASELINE_IDEOGRAPHC;
      break;
    case NS_STYLE_DOMINANT_BASELINE_HANGING:
      baseline = nsISVGRendererGlyphMetrics::BASELINE_HANGING;
      break;
    case NS_STYLE_DOMINANT_BASELINE_AUTO:
    case NS_STYLE_DOMINANT_BASELINE_USE_SCRIPT:
    case NS_STYLE_DOMINANT_BASELINE_ALPHABETIC:
    default:
      baseline = nsISVGRendererGlyphMetrics::BASELINE_ALPHABETIC;
      break;
  }

  nsISVGGlyphFragmentLeaf *fragment, *firstFragment;

  firstFragment = node->GetFirstGlyphFragment();

  float x = 0, y = 0;

  {
    nsCOMPtr<nsIDOMSVGLengthList> list = GetX();
    GetSingleValue(firstFragment, list, &x);
  }
  {
    nsCOMPtr<nsIDOMSVGLengthList> list = GetY();
    GetSingleValue(firstFragment, list, &y);
  }

  // loop over chunks
  while (firstFragment) {
    {
      nsCOMPtr<nsIDOMSVGLengthList> list = firstFragment->GetX();
      GetSingleValue(firstFragment, list, &x);
    }
    {
      nsCOMPtr<nsIDOMSVGLengthList> list = firstFragment->GetY();
      GetSingleValue(firstFragment, list, &y);
    }

    // determine x offset based on text_anchor:
  
    PRUint8 anchor = firstFragment->GetTextAnchor();

    float chunkLength = 0.0f;
    if (anchor != NS_STYLE_TEXT_ANCHOR_START) {
      // need to get the total chunk length
    
      fragment = firstFragment;
      while (fragment) {
        nsCOMPtr<nsISVGRendererGlyphMetrics> metrics;
        fragment->GetGlyphMetrics(getter_AddRefs(metrics));
        if (!metrics) continue;

        float advance, dx = 0.0f;
        nsCOMPtr<nsIDOMSVGLengthList> list = fragment->GetDx();
        GetSingleValue(fragment, list, &dx);
        metrics->GetAdvance(&advance);
        chunkLength += advance + dx;
        fragment = fragment->GetNextGlyphFragment();
        if (fragment && fragment->IsAbsolutelyPositioned())
          break;
      }
    }

    if (anchor == NS_STYLE_TEXT_ANCHOR_MIDDLE)
      x -= chunkLength/2.0f;
    else if (anchor == NS_STYLE_TEXT_ANCHOR_END)
      x -= chunkLength;
  
    // set position of each fragment in this chunk:
  
    fragment = firstFragment;
    while (fragment) {
      nsCOMPtr<nsISVGRendererGlyphMetrics> metrics;
      fragment->GetGlyphMetrics(getter_AddRefs(metrics));
      if (!metrics) continue;

      float baseline_offset, dx = 0.0f, dy = 0.0f;
      metrics->GetBaselineOffset(baseline, &baseline_offset);
      {
        nsCOMPtr<nsIDOMSVGLengthList> list = fragment->GetDx();
        GetSingleValue(fragment, list, &dx);
      }
      {
        nsCOMPtr<nsIDOMSVGLengthList> list = fragment->GetDy();
        GetSingleValue(fragment, list, &dy);
      }

      fragment->SetGlyphPosition(x + dx, y + dy - baseline_offset);

      float advance;
      metrics->GetAdvance(&advance);
      x += dx + advance;
      y += dy;
      fragment = fragment->GetNextGlyphFragment();
      if (fragment && fragment->IsAbsolutelyPositioned())
        break;
    }
    firstFragment = fragment;
  }

  mPositioningDirty = PR_FALSE;
}


NS_IMETHODIMP_(already_AddRefed<nsIDOMSVGLengthList>)
nsSVGTextFrame::GetX()
{
  nsCOMPtr<nsIDOMSVGTextPositioningElement> tpElement = do_QueryInterface(mContent);
  NS_ASSERTION(tpElement, "wrong content element");

  nsCOMPtr<nsIDOMSVGAnimatedLengthList> animLengthList;
  tpElement->GetX(getter_AddRefs(animLengthList));
  nsIDOMSVGLengthList *retval;
  animLengthList->GetAnimVal(&retval);
  return retval;
}

NS_IMETHODIMP_(already_AddRefed<nsIDOMSVGLengthList>)
nsSVGTextFrame::GetY()
{
  nsCOMPtr<nsIDOMSVGTextPositioningElement> tpElement = do_QueryInterface(mContent);
  NS_ASSERTION(tpElement, "wrong content element");

  nsCOMPtr<nsIDOMSVGAnimatedLengthList> animLengthList;
  tpElement->GetY(getter_AddRefs(animLengthList));
  nsIDOMSVGLengthList *retval;
  animLengthList->GetAnimVal(&retval);
  return retval;
}

NS_IMETHODIMP_(already_AddRefed<nsIDOMSVGLengthList>)
nsSVGTextFrame::GetDx()
{
  nsCOMPtr<nsIDOMSVGTextPositioningElement> tpElement = do_QueryInterface(mContent);
  NS_ASSERTION(tpElement, "wrong content element");

  nsCOMPtr<nsIDOMSVGAnimatedLengthList> animLengthList;
  tpElement->GetDx(getter_AddRefs(animLengthList));
  nsIDOMSVGLengthList *retval;
  animLengthList->GetAnimVal(&retval);
  return retval;
}

NS_IMETHODIMP_(already_AddRefed<nsIDOMSVGLengthList>)
nsSVGTextFrame::GetDy()
{
  nsCOMPtr<nsIDOMSVGTextPositioningElement> tpElement = do_QueryInterface(mContent);
  NS_ASSERTION(tpElement, "wrong content element");

  nsCOMPtr<nsIDOMSVGAnimatedLengthList> animLengthList;
  tpElement->GetDy(getter_AddRefs(animLengthList));
  nsIDOMSVGLengthList *retval;
  animLengthList->GetAnimVal(&retval);
  return retval;
}
