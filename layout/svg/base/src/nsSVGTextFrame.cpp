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

#include "nsContainerFrame.h"
#include "nsIDOMSVGTextElement.h"
#include "nsPresContext.h"
#include "nsISVGTextFrame.h"
#include "nsISVGRendererCanvas.h"
#include "nsWeakReference.h"
#include "nsISVGValue.h"
#include "nsISVGValueObserver.h"
#include "nsIDOMSVGTransformable.h"
#include "nsIDOMSVGAnimTransformList.h"
#include "nsIDOMSVGSVGElement.h"
#include "nsIDOMSVGMatrix.h"
#include "nsIDOMSVGLengthList.h"
#include "nsIDOMSVGLength.h"
#include "nsISVGValueUtils.h"
#include "nsIDOMSVGAnimatedLengthList.h"
#include "nsIDOMSVGTransformList.h"
#include "nsISVGContainerFrame.h"
#include "nsISVGChildFrame.h"
#include "nsISVGGlyphFragmentNode.h"
#include "nsISVGGlyphFragmentLeaf.h"
#include "nsISVGRendererGlyphMetrics.h"
#include "nsISVGOuterSVGFrame.h"
#include "nsIDOMSVGRect.h"
#include "nsISVGTextContentMetrics.h"

typedef nsContainerFrame nsSVGTextFrameBase;

class nsSVGTextFrame : public nsSVGTextFrameBase,
                       public nsISVGTextFrame, // : nsISVGTextContainerFrame
                       public nsISVGChildFrame,
                       public nsISVGContainerFrame,
                       public nsISVGValueObserver,
                       public nsISVGTextContentMetrics,
                       public nsSupportsWeakReference
{
  friend nsresult
  NS_NewSVGTextFrame(nsIPresShell* aPresShell, nsIContent* aContent,
                     nsIFrame** aNewFrame);
protected:
  nsSVGTextFrame();
  virtual ~nsSVGTextFrame();
  nsresult Init();
  
   // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }  
public:
  // nsIFrame:

  NS_IMETHOD  AppendFrames(nsPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aFrameList);
  NS_IMETHOD  InsertFrames(nsPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aPrevFrame,
                           nsIFrame*       aFrameList);
  NS_IMETHOD  RemoveFrame(nsPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aOldFrame);
  NS_IMETHOD  ReplaceFrame(nsPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aOldFrame,
                           nsIFrame*       aNewFrame);
  
  NS_IMETHOD Init(nsPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsStyleContext*  aContext,
                  nsIFrame*        aPrevInFlow);

  NS_IMETHOD  AttributeChanged(nsPresContext* aPresContext,
                               nsIContent*     aChild,
                               PRInt32         aNameSpaceID,
                               nsIAtom*        aAttribute,
                               PRInt32         aModType,
                               PRInt32         aHint);

  NS_IMETHOD DidSetStyleContext(nsPresContext* aPresContext);

  // nsISVGValueObserver
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable);
  NS_IMETHOD DidModifySVGObservable (nsISVGValue* observable);

  // nsISVGTextContentMetrics
  NS_IMETHOD GetExtentOfChar(PRUint32 charnum, nsIDOMSVGRect **_retval);
  
  // nsISupportsWeakReference
  // implementation inherited from nsSupportsWeakReference
  
  // nsISVGChildFrame interface:
  NS_IMETHOD Paint(nsISVGRendererCanvas* canvas, const nsRect& dirtyRectTwips);
  NS_IMETHOD GetFrameForPoint(float x, float y, nsIFrame** hit);
  NS_IMETHOD_(already_AddRefed<nsISVGRendererRegion>) GetCoveredRegion();
  NS_IMETHOD InitialUpdate();
  NS_IMETHOD NotifyCanvasTMChanged();
  NS_IMETHOD NotifyRedrawSuspended();
  NS_IMETHOD NotifyRedrawUnsuspended();
  NS_IMETHOD GetBBox(nsIDOMSVGRect **_retval);
  
  // nsISVGContainerFrame interface:
  nsISVGOuterSVGFrame *GetOuterSVGFrame();
  already_AddRefed<nsIDOMSVGMatrix> GetCanvasTM();
  already_AddRefed<nsSVGCoordCtxProvider> GetCoordContextProvider();
  
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

protected:
  void EnsureFragmentTreeUpToDate();
  void UpdateFragmentTree();
  void UpdateGlyphPositioning();
  already_AddRefed<nsIDOMSVGLengthList> GetX();
  already_AddRefed<nsIDOMSVGLengthList> GetY();
  already_AddRefed<nsIDOMSVGAnimatedTransformList> GetTransform();
  nsISVGGlyphFragmentNode *GetFirstGlyphFragmentChildNode();
  nsISVGGlyphFragmentNode *GetNextGlyphFragmentChildNode(nsISVGGlyphFragmentNode*node);
  nsISVGGlyphFragmentLeaf *GetGlyphFragmentAtCharNum(PRUint32 charnum);

  enum UpdateState{
    unsuspended,
    suspended,
    updating};
  UpdateState mMetricsState;
  UpdateState mFragmentTreeState;
  PRBool mPositioningDirty;
  PRBool mFragmentTreeDirty;

  nsCOMPtr<nsIDOMSVGMatrix> mCanvasTM;
};

//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGTextFrame(nsIPresShell* aPresShell, nsIContent* aContent,
                   nsIFrame** aNewFrame)
{
  *aNewFrame = nsnull;

  nsCOMPtr<nsIDOMSVGTextElement> text_elem = do_QueryInterface(aContent);
  if (!text_elem) {
#ifdef DEBUG
    printf("warning: trying to construct an SVGTextFrame for a "
           "content element that doesn't support the right interfaces\n");
#endif
    return NS_ERROR_FAILURE;
  }
  
  nsSVGTextFrame* it = new (aPresShell) nsSVGTextFrame;
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  
  return NS_OK;
}

nsSVGTextFrame::nsSVGTextFrame()
    : mFragmentTreeState(suspended), mMetricsState(suspended),
      mFragmentTreeDirty(PR_FALSE), mPositioningDirty(PR_FALSE)
{
}

nsSVGTextFrame::~nsSVGTextFrame()
{
  // clean up our listener refs:
  {
    nsCOMPtr<nsIDOMSVGLengthList> lengthList = GetX();
    NS_REMOVE_SVGVALUE_OBSERVER(lengthList);
  }

  {
    nsCOMPtr<nsIDOMSVGLengthList> lengthList = GetY();
    NS_REMOVE_SVGVALUE_OBSERVER(lengthList);
  }

  {
    nsCOMPtr<nsIDOMSVGTransformable> transformable = do_QueryInterface(mContent);
    NS_ASSERTION(transformable, "wrong content element");
    nsCOMPtr<nsIDOMSVGAnimatedTransformList> transforms;
    transformable->GetTransform(getter_AddRefs(transforms));
    NS_REMOVE_SVGVALUE_OBSERVER(transforms);
  }
}

nsresult nsSVGTextFrame::Init()
{
  // set us up as a listener for various <text>-properties: 
  {
    nsCOMPtr<nsIDOMSVGLengthList> lengthList = GetX();
    NS_ADD_SVGVALUE_OBSERVER(lengthList);
  }

  {
    nsCOMPtr<nsIDOMSVGLengthList> lengthList = GetY();
    NS_ADD_SVGVALUE_OBSERVER(lengthList);
  }

  {
    nsCOMPtr<nsIDOMSVGAnimatedTransformList> transforms = GetTransform();
    NS_ADD_SVGVALUE_OBSERVER(transforms);
  }
  
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN(nsSVGTextFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGTextFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGTextContainerFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGContainerFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGChildFrame)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
  NS_INTERFACE_MAP_ENTRY(nsISVGTextContentMetrics)
NS_INTERFACE_MAP_END_INHERITING(nsSVGTextFrameBase)


//----------------------------------------------------------------------
// nsIFrame methods
NS_IMETHODIMP
nsSVGTextFrame::Init(nsPresContext*  aPresContext,
                     nsIContent*      aContent,
                     nsIFrame*        aParent,
                     nsStyleContext*  aContext,
                     nsIFrame*        aPrevInFlow)
{
  nsresult rv;
  rv = nsSVGTextFrameBase::Init(aPresContext, aContent, aParent,
                                aContext, aPrevInFlow);

  Init();
  
  return rv;
}

NS_IMETHODIMP
nsSVGTextFrame::AttributeChanged(nsPresContext* aPresContext,
                                 nsIContent*     aChild,
                                 PRInt32         aNameSpaceID,
                                 nsIAtom*        aAttribute,
                                 PRInt32         aModType,
                                 PRInt32         aHint)
{
  // we don't use this notification mechanism
  
#ifdef DEBUG
  printf("** nsSVGTextFrame::AttributeChanged(");
  nsAutoString str;
  aAttribute->ToString(str);
  printf(NS_ConvertUTF16toUTF8(str).get());
  printf(")\n");
#endif
  
  return NS_OK;
}

NS_IMETHODIMP
nsSVGTextFrame::DidSetStyleContext(nsPresContext* aPresContext)
{
#ifdef DEBUG
  printf("** nsSVGTextFrame::DidSetStyleContext\n");
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsSVGTextFrame::AppendFrames(nsPresContext* aPresContext,
                             nsIPresShell&   aPresShell,
                             nsIAtom*        aListName,
                             nsIFrame*       aFrameList)
{
  // append == insert at end:
  return InsertFrames(aPresContext, aPresShell, aListName,
                      mFrames.LastChild(), aFrameList);  
}

NS_IMETHODIMP
nsSVGTextFrame::InsertFrames(nsPresContext* aPresContext,
                             nsIPresShell&   aPresShell,
                             nsIAtom*        aListName,
                             nsIFrame*       aPrevFrame,
                             nsIFrame*       aFrameList)
{
  // memorize last new frame
  nsIFrame* lastNewFrame = nsnull;
  {
    nsFrameList tmpList(aFrameList);
    lastNewFrame = tmpList.LastChild();
  }
  
  // Insert the new frames
  mFrames.InsertFrames(this, aPrevFrame, aFrameList);

  // call InitialUpdate() on all new frames:
  nsIFrame* kid = aFrameList;
  nsIFrame* end = nsnull;
  if (lastNewFrame)
    end = lastNewFrame->GetNextSibling();
  
  while (kid != end) {
    nsISVGChildFrame* SVGFrame=nsnull;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      SVGFrame->InitialUpdate(); 
    }
    kid = kid->GetNextSibling();
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsSVGTextFrame::RemoveFrame(nsPresContext* aPresContext,
                            nsIPresShell&   aPresShell,
                            nsIAtom*        aListName,
                            nsIFrame*       aOldFrame)
{
  nsCOMPtr<nsISVGRendererRegion> dirty_region;

  nsISVGChildFrame* SVGFrame=nsnull;
  aOldFrame->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);

  if (SVGFrame)
    dirty_region = SVGFrame->GetCoveredRegion();
  
  PRBool result = mFrames.DestroyFrame(aPresContext, aOldFrame);

  nsISVGOuterSVGFrame* outerSVGFrame = GetOuterSVGFrame();
  NS_ASSERTION(outerSVGFrame, "no outer svg frame");

  if (SVGFrame && outerSVGFrame) {
    // XXX We need to rebuild the fragment tree starting from the
    // removed frame. Let's just rebuild the whole tree for now
    outerSVGFrame->SuspendRedraw();
    mFragmentTreeDirty = PR_TRUE;
    
    if (dirty_region) {
      outerSVGFrame->InvalidateRegion(dirty_region, PR_FALSE);
    }

    outerSVGFrame->UnsuspendRedraw();
  }
  
  NS_ASSERTION(result, "didn't find frame to delete");
  return result ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSVGTextFrame::ReplaceFrame(nsPresContext* aPresContext,
                             nsIPresShell&   aPresShell,
                             nsIAtom*        aListName,
                             nsIFrame*       aOldFrame,
                             nsIFrame*       aNewFrame)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods:

NS_IMETHODIMP
nsSVGTextFrame::WillModifySVGObservable(nsISVGValue* observable)
{
  return NS_OK;
}


NS_IMETHODIMP
nsSVGTextFrame::DidModifySVGObservable (nsISVGValue* observable)
{  
  nsCOMPtr<nsIDOMSVGAnimatedTransformList> transforms = GetTransform();
  if (SameCOMIdentity(observable, transforms)) {
    // transform has changed

    // make sure our cached transform matrix gets (lazily) updated
    mCanvasTM = nsnull;
    
    nsIFrame* kid = mFrames.FirstChild();
    while (kid) {
      nsISVGChildFrame* SVGFrame=0;
      kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
      if (SVGFrame)
        SVGFrame->NotifyCanvasTMChanged();
      kid = kid->GetNextSibling();
    }
  }
  else {
    // x, y have changed
    mPositioningDirty = PR_TRUE;
    if (mMetricsState == unsuspended) {
      UpdateGlyphPositioning();
    }
  }
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGTextContentMetrics
NS_IMETHODIMP
nsSVGTextFrame::GetExtentOfChar(PRUint32 charnum, nsIDOMSVGRect **_retval)
{
  *_retval = nsnull;
  
  EnsureFragmentTreeUpToDate();

  nsISVGGlyphFragmentLeaf *fragment = GetGlyphFragmentAtCharNum(charnum);
  if (!fragment) return NS_ERROR_FAILURE; // xxx return some index-out-of-range error
  
  // query the renderer metrics for the bounds of the character
  nsCOMPtr<nsISVGRendererGlyphMetrics> metrics;
  fragment->GetGlyphMetrics(getter_AddRefs(metrics));
  NS_ASSERTION(metrics, "null metrics");
  nsresult rv = metrics->GetExtentOfChar(charnum-fragment->GetCharNumberOffset(),
                                         _retval);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  // offset the bounds by the position of the fragment:
  float x,y;
  (*_retval)->GetX(&x);
  (*_retval)->GetY(&y);
  (*_retval)->SetX(x+fragment->GetGlyphPositionX());
  (*_retval)->SetY(y+fragment->GetGlyphPositionY());

  return NS_OK;
}


//----------------------------------------------------------------------
// nsISVGChildFrame methods

NS_IMETHODIMP
nsSVGTextFrame::Paint(nsISVGRendererCanvas* canvas, const nsRect& dirtyRectTwips)
{
#ifdef DEBUG
//  printf("nsSVGTextFrame(%p)::Paint\n", this);
#endif

  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGChildFrame* SVGFrame=0;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame)
      SVGFrame->Paint(canvas, dirtyRectTwips);
    kid = kid->GetNextSibling();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSVGTextFrame::GetFrameForPoint(float x, float y, nsIFrame** hit)
{
#ifdef DEBUG
//  printf("nsSVGTextFrame(%p)::GetFrameForPoint\n", this);
#endif
  *hit = nsnull;
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGChildFrame* SVGFrame=0;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      nsIFrame* temp=nsnull;
      nsresult rv = SVGFrame->GetFrameForPoint(x, y, &temp);
      if (NS_SUCCEEDED(rv) && temp) {
        *hit = temp;
        // return NS_OK; can't return. we need reverse order but only
        // have a singly linked list...
      }
    }
    kid = kid->GetNextSibling();
  }
  
  return *hit ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP_(already_AddRefed<nsISVGRendererRegion>)
nsSVGTextFrame::GetCoveredRegion()
{
  nsISVGRendererRegion *accu_region=nsnull;
  
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGChildFrame* SVGFrame=0;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      nsCOMPtr<nsISVGRendererRegion> dirty_region = SVGFrame->GetCoveredRegion();
      if (dirty_region) {
        if (accu_region) {
          nsCOMPtr<nsISVGRendererRegion> temp = dont_AddRef(accu_region);
          dirty_region->Combine(temp, &accu_region);
        }
        else {
          accu_region = dirty_region;
          NS_IF_ADDREF(accu_region);
        }
      }
    }
    kid = kid->GetNextSibling();
  }
  
  return accu_region;
}

NS_IMETHODIMP
nsSVGTextFrame::InitialUpdate()
{
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGChildFrame* SVGFrame=0;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      SVGFrame->InitialUpdate();
    }
    kid = kid->GetNextSibling();
  }

#ifdef DEBUG
  nsISVGOuterSVGFrame *outerSVGFrame = GetOuterSVGFrame();
  if (!outerSVGFrame) {
    NS_ERROR("null outerSVGFrame");
    return NS_ERROR_FAILURE;
  }
  
  PRBool suspended;
  outerSVGFrame->IsRedrawSuspended(&suspended);
  if (!suspended) NS_ERROR("initialupdate while redraw not suspended! need to update fragment tree");
  //XXX
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsSVGTextFrame::NotifyCanvasTMChanged()
{
  // make sure our cached transform matrix gets (lazily) updated
  mCanvasTM = nsnull;
  
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGChildFrame* SVGFrame=0;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      SVGFrame->NotifyCanvasTMChanged();
    }
    kid = kid->GetNextSibling();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGTextFrame::NotifyRedrawSuspended()
{
  mMetricsState = suspended;
  mFragmentTreeState = suspended;
  
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGChildFrame* SVGFrame=nsnull;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      SVGFrame->NotifyRedrawSuspended();
    }
    nsISVGGlyphFragmentNode* fragmentNode=nsnull;
    kid->QueryInterface(NS_GET_IID(nsISVGGlyphFragmentNode), (void**)&fragmentNode);
    if (fragmentNode) {
      fragmentNode->NotifyMetricsSuspended();
      fragmentNode->NotifyGlyphFragmentTreeSuspended();
    }
    kid = kid->GetNextSibling();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGTextFrame::NotifyRedrawUnsuspended()
{
  NS_ASSERTION(mMetricsState == suspended, "metrics state not suspended during redraw");
  NS_ASSERTION(mFragmentTreeState == suspended, "fragment tree not suspended during redraw");

  // 3 passes:
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
  if (mFragmentTreeDirty)
    UpdateFragmentTree();
  
  mMetricsState = updating;
  kid = mFrames.FirstChild();
  while (kid) {
    nsISVGGlyphFragmentNode* node=nsnull;
    kid->QueryInterface(NS_GET_IID(nsISVGGlyphFragmentNode), (void**)&node);
    if (node)
      node->NotifyMetricsUnsuspended();
    kid = kid->GetNextSibling();
  }

  mMetricsState = unsuspended;
  if (mPositioningDirty)
    UpdateGlyphPositioning();
  
  kid = mFrames.FirstChild();
  while (kid) {
    nsISVGChildFrame* SVGFrame=nsnull;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      SVGFrame->NotifyRedrawUnsuspended();
    }
    kid = kid->GetNextSibling();
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsSVGTextFrame::GetBBox(nsIDOMSVGRect **_retval)
{
  *_retval = nsnull;
  
  // iterate over all children and accumulate the bounding rect:
  // this relies on the fact that children of <text> elements can't
  // have individual transforms

  EnsureFragmentTreeUpToDate();
  
  float x1=0.0f, y1=0.0f, x2=0.0f, y2=0.0f;
  PRBool bFirst=PR_TRUE;
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGChildFrame* SVGFrame=0;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      nsCOMPtr<nsIDOMSVGRect> r;
      SVGFrame->GetBBox(getter_AddRefs(r));
      NS_ASSERTION(r, "no bounding box");
      if (!r) continue;

      float x,y,w,h;
      r->GetX(&x);
      r->GetY(&y);
      r->GetWidth(&w);
      r->GetHeight(&h);
      
      if (bFirst) {
        bFirst = PR_FALSE;
        x1 = x;
        y1 = y;
        x2 = x+w;
        y2 = y+h;
      }
      else {
        if (x<x1) x1 = x;
        if (x+w>x2) x2 = x+w;
        if (y<y1) y1 = y;
        if (y+h>y2) y2 = y+h;
      }
    }
    kid = kid->GetNextSibling();
  }

  nsISVGOuterSVGFrame *outerSVGFrame = GetOuterSVGFrame();
  if (!outerSVGFrame) {
    NS_ERROR("null outerSVGFrame");
    return NS_ERROR_FAILURE;
  }

  outerSVGFrame->CreateSVGRect(_retval);

  (*_retval)->SetWidth(x2-x1);
  (*_retval)->SetHeight(y2-y1);
  (*_retval)->SetX(x1);
  (*_retval)->SetY(y1);

  return NS_OK;  
}

//----------------------------------------------------------------------
// nsISVGContainerFrame methods:

nsISVGOuterSVGFrame *
nsSVGTextFrame::GetOuterSVGFrame()
{
  NS_ASSERTION(mParent, "null parent");
  
  nsISVGContainerFrame *containerFrame;
  mParent->QueryInterface(NS_GET_IID(nsISVGContainerFrame), (void**)&containerFrame);
  if (!containerFrame) {
    NS_ERROR("invalid container");
    return nsnull;
  }

  return containerFrame->GetOuterSVGFrame();  
}

already_AddRefed<nsIDOMSVGMatrix>
nsSVGTextFrame::GetCanvasTM()
{
  if (!mCanvasTM) {
    // get our parent's tm and append local transforms (if any):
    NS_ASSERTION(mParent, "null parent");
    nsISVGContainerFrame *containerFrame;
    mParent->QueryInterface(NS_GET_IID(nsISVGContainerFrame), (void**)&containerFrame);
    if (!containerFrame) {
      NS_ERROR("invalid parent");
      return nsnull;
    }
    nsCOMPtr<nsIDOMSVGMatrix> parentTM = containerFrame->GetCanvasTM();
    NS_ASSERTION(parentTM, "null TM");

    // got the parent tm, now check for local tm:
    nsCOMPtr<nsIDOMSVGMatrix> localTM;
    {
      nsCOMPtr<nsIDOMSVGTransformable> transformable = do_QueryInterface(mContent);
      NS_ASSERTION(transformable, "wrong content element");
      nsCOMPtr<nsIDOMSVGAnimatedTransformList> atl;
      transformable->GetTransform(getter_AddRefs(atl));
      NS_ASSERTION(atl, "null animated transform list");
      nsCOMPtr<nsIDOMSVGTransformList> transforms;
      atl->GetAnimVal(getter_AddRefs(transforms));
      NS_ASSERTION(transforms, "null transform list");
      PRUint32 numberOfItems;
      transforms->GetNumberOfItems(&numberOfItems);
      if (numberOfItems>0)
        transforms->GetConsolidation(getter_AddRefs(localTM));
    }
    
    if (localTM)
      parentTM->Multiply(localTM, getter_AddRefs(mCanvasTM));
    else
      mCanvasTM = parentTM;
  }

  nsIDOMSVGMatrix* retval = mCanvasTM.get();
  NS_IF_ADDREF(retval);
  return retval;
}

already_AddRefed<nsSVGCoordCtxProvider>
nsSVGTextFrame::GetCoordContextProvider()
{
  NS_ASSERTION(mParent, "null parent");
  
  nsISVGContainerFrame *containerFrame;
  mParent->QueryInterface(NS_GET_IID(nsISVGContainerFrame), (void**)&containerFrame);
  if (!containerFrame) {
    NS_ERROR("invalid container");
    return nsnull;
  }

  return containerFrame->GetCoordContextProvider();  
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

  PRUint32 charNum = 0;
  
  nsISVGGlyphFragmentNode* node = GetFirstGlyphFragmentChildNode();
  nsISVGGlyphFragmentNode* next;
  while (node) {
    next = GetNextGlyphFragmentChildNode(node);
    charNum = node->BuildGlyphFragmentTree(charNum, !next);
    node = next;
  }

  mFragmentTreeDirty = PR_FALSE;
  
  mPositioningDirty = PR_TRUE;
  if (mMetricsState == unsuspended)
    UpdateGlyphPositioning();
}

void
nsSVGTextFrame::UpdateGlyphPositioning()
{
  // XXX need to iterate separately over each chunk (currently we
  // treat everything underneath this text-element as a single chunk)
  
  NS_ASSERTION(mMetricsState == unsuspended, "updating during suspension");

  nsISVGGlyphFragmentNode *node = GetFirstGlyphFragmentChildNode();
  if (!node) return;

  nsISVGGlyphFragmentLeaf* fragment;

  float x=0.0f;
  {
    nsCOMPtr<nsIDOMSVGLengthList> list = GetX();
    PRUint32 count = 0;
    list->GetNumberOfItems(&count);
#ifdef DEBUG
    if (count > 1)
      NS_WARNING("multiple lengths for x/y attributes on <text> elements not implemented yet!");
#endif
    if (count) {
      nsCOMPtr<nsIDOMSVGLength> length;
      list->GetItem(0, getter_AddRefs(length));
      length->GetValue(&x);
    }
  }
  
  float y=0.0f;
  {
    nsCOMPtr<nsIDOMSVGLengthList> list = GetY();
    PRUint32 count = 0;
    list->GetNumberOfItems(&count);
#ifdef DEBUG
    if (count > 1)
      NS_WARNING("multiple lengths for x/y attributes on <text> elements not implemented yet!");
#endif
    if (count) {
      nsCOMPtr<nsIDOMSVGLength> length;
      list->GetItem(0, getter_AddRefs(length));
      length->GetValue(&y);
    }
  }

  // determine x offset based on text_anchor:
  
  PRUint8 anchor = ((const nsStyleSVG*)mStyleContext->GetStyleData(eStyleStruct_SVG))->mTextAnchor;

  float chunkLength = 0.0f;
  if (anchor != NS_STYLE_TEXT_ANCHOR_START) {
    // need to get the total chunk length
    
    fragment = node->GetFirstGlyphFragment();
    while (fragment) {
      nsCOMPtr<nsISVGRendererGlyphMetrics> metrics;
      fragment->GetGlyphMetrics(getter_AddRefs(metrics));
      NS_ASSERTION(metrics, "null metrics");
      float advance;
      metrics->GetAdvance(&advance);
      chunkLength+=advance;
      fragment = fragment->GetNextGlyphFragment();
    }
  }

  if (anchor == NS_STYLE_TEXT_ANCHOR_MIDDLE) 
    x -= chunkLength/2.0f;
  else if (anchor == NS_STYLE_TEXT_ANCHOR_END)
    x -= chunkLength;

  // we'll align every fragment in this chunk on the dominant-baseline:
  // XXX should actually inspect 'alignment-baseline' for each fragment
  
  PRUint8 baseline;
  switch(((const nsStyleSVGReset*)mStyleContext->GetStyleData(eStyleStruct_SVGReset))->mDominantBaseline) {
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
  
  // set position of each fragment in this chunk:
  
  fragment = node->GetFirstGlyphFragment();
  while (fragment) {
    nsCOMPtr<nsISVGRendererGlyphMetrics> metrics;
    fragment->GetGlyphMetrics(getter_AddRefs(metrics));
    NS_ASSERTION(metrics, "null metrics");

    float baseline_offset;
    metrics->GetBaselineOffset(baseline, &baseline_offset);

    fragment->SetGlyphPosition(x, y - baseline_offset);

    float advance;
    metrics->GetAdvance(&advance);
    x+=advance;
    fragment = fragment->GetNextGlyphFragment();
  }

  mPositioningDirty = PR_FALSE;
}


already_AddRefed<nsIDOMSVGLengthList>
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

already_AddRefed<nsIDOMSVGLengthList>
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

already_AddRefed<nsIDOMSVGAnimatedTransformList>
nsSVGTextFrame::GetTransform()
{
  nsCOMPtr<nsIDOMSVGTransformable> transformable = do_QueryInterface(mContent);
  NS_ASSERTION(transformable, "wrong content element");
  
  nsIDOMSVGAnimatedTransformList *retval;
  transformable->GetTransform(&retval);
  return retval;
}

nsISVGGlyphFragmentNode *
nsSVGTextFrame::GetFirstGlyphFragmentChildNode()
{
  nsISVGGlyphFragmentNode* retval = nsnull;
  nsIFrame* frame = mFrames.FirstChild();
  while (frame) {
    frame->QueryInterface(NS_GET_IID(nsISVGGlyphFragmentNode),(void**)&retval);
    if (retval) break;
    frame = frame->GetNextSibling();
  }
  return retval;
}

nsISVGGlyphFragmentNode *
nsSVGTextFrame::GetNextGlyphFragmentChildNode(nsISVGGlyphFragmentNode*node)
{
  nsISVGGlyphFragmentNode* retval = nsnull;
  nsIFrame* frame = nsnull;
  node->QueryInterface(NS_GET_IID(nsIFrame), (void**)&frame);
  NS_ASSERTION(frame, "interface not implemented");
  frame = frame->GetNextSibling();
  while (frame) {
    frame->QueryInterface(NS_GET_IID(nsISVGGlyphFragmentNode),(void**)&retval);
    if (retval) break;
    frame = frame->GetNextSibling();
  }
  return retval;
}

nsISVGGlyphFragmentLeaf *
nsSVGTextFrame::GetGlyphFragmentAtCharNum(PRUint32 charnum)
{
  nsISVGGlyphFragmentLeaf *fragment = nsnull;
  {
    nsISVGGlyphFragmentNode* node = GetFirstGlyphFragmentChildNode();
    if (!node) return nsnull; 
    fragment = node->GetFirstGlyphFragment();
  }
  
  while(fragment) {
    PRUint32 count = fragment->GetNumberOfChars();
    if (count>charnum)
      return fragment;
    charnum-=count;
    fragment = fragment->GetNextGlyphFragment();
  }

  // not found
  return nsnull;
}
