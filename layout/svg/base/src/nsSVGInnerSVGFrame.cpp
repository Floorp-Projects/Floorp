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
 * Portions created by the Initial Developer are Copyright (C) 2001
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
#include "nsIDOMSVGGElement.h"
#include "nsPresContext.h"
#include "nsISVGChildFrame.h"
#include "nsISVGContainerFrame.h"
#include "nsISVGRendererCanvas.h"
#include "nsISVGOuterSVGFrame.h"
#include "nsIDOMSVGSVGElement.h"
#include "nsISVGSVGElement.h"

typedef nsContainerFrame nsSVGInnerSVGFrameBase;

class nsSVGInnerSVGFrame : public nsSVGInnerSVGFrameBase,
                                   public nsISVGChildFrame,
                                   public nsISVGContainerFrame,
                                   public nsISVGSVGFrame
{
  friend nsresult
  NS_NewSVGInnerSVGFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame);
protected:
  nsSVGInnerSVGFrame();
  virtual ~nsSVGInnerSVGFrame();
  nsresult Init();
  
   // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }  
public:
  // nsIFrame:
  
  NS_IMETHOD  AppendFrames(nsPresContext* aPresContext,
                           nsIPresShell&  aPresShell,
                           nsIAtom*       aListName,
                           nsIFrame*      aFrameList);
  NS_IMETHOD  InsertFrames(nsPresContext* aPresContext,
                           nsIPresShell&  aPresShell,
                           nsIAtom*       aListName,
                           nsIFrame*      aPrevFrame,
                           nsIFrame*      aFrameList);
  NS_IMETHOD  RemoveFrame(nsPresContext* aPresContext,
                          nsIPresShell&  aPresShell,
                          nsIAtom*       aListName,
                          nsIFrame*      aOldFrame);
  NS_IMETHOD  ReplaceFrame(nsPresContext* aPresContext,
                           nsIPresShell&  aPresShell,
                           nsIAtom*       aListName,
                           nsIFrame*      aOldFrame,
                           nsIFrame*      aNewFrame);
  NS_IMETHOD Init(nsPresContext*  aPresContext,
                  nsIContent*     aContent,
                  nsIFrame*       aParent,
                  nsStyleContext* aContext,
                  nsIFrame*       aPrevInFlow);

  NS_IMETHOD  AttributeChanged(nsPresContext* aPresContext,
                               nsIContent*    aChild,
                               PRInt32        aNameSpaceID,
                               nsIAtom*       aAttribute,
                               PRInt32        aModType);


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
  nsISVGOuterSVGFrame*GetOuterSVGFrame();
  already_AddRefed<nsIDOMSVGMatrix> GetCanvasTM();
  already_AddRefed<nsSVGCoordCtxProvider> GetCoordContextProvider();

  // nsISVGSVGFrame interface:
  NS_IMETHOD SuspendRedraw();
  NS_IMETHOD UnsuspendRedraw();
  NS_IMETHOD NotifyViewportChange();

protected:
  nsCOMPtr<nsIDOMSVGMatrix> mCanvasTM;
};

//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGInnerSVGFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame)
{
  nsSVGInnerSVGFrame* it = new (aPresShell) nsSVGInnerSVGFrame;
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;

  return NS_OK;
}

nsSVGInnerSVGFrame::nsSVGInnerSVGFrame()
{
#ifdef DEBUG
//  printf("nsSVGInnerSVGFrame CTOR\n");
#endif
}

nsSVGInnerSVGFrame::~nsSVGInnerSVGFrame()
{
#ifdef DEBUG
//  printf("~nsSVGInnerSVGFrame\n");
#endif
}

nsresult nsSVGInnerSVGFrame::Init()
{
  NS_ASSERTION(mParent, "no parent");
  
  // hook up CoordContextProvider chain:
  
  nsISVGContainerFrame *containerFrame;
  mParent->QueryInterface(NS_GET_IID(nsISVGContainerFrame), (void**)&containerFrame);
  if (!containerFrame) {
    NS_ERROR("invalid container");
    return NS_ERROR_FAILURE;
  }
  
  nsCOMPtr<nsISVGSVGElement> SVGElement = do_QueryInterface(mContent);
  NS_ASSERTION(SVGElement, "wrong content element");
  SVGElement->SetParentCoordCtxProvider(nsRefPtr<nsSVGCoordCtxProvider>(containerFrame->GetCoordContextProvider()));
  
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN(nsSVGInnerSVGFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGChildFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGContainerFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGSVGFrame)
NS_INTERFACE_MAP_END_INHERITING(nsSVGInnerSVGFrameBase)


//----------------------------------------------------------------------
// nsIFrame methods
NS_IMETHODIMP
nsSVGInnerSVGFrame::Init(nsPresContext*  aPresContext,
                         nsIContent*     aContent,
                         nsIFrame*       aParent,
                         nsStyleContext* aContext,
                         nsIFrame*       aPrevInFlow)
{
  nsresult rv;
  rv = nsSVGInnerSVGFrameBase::Init(aPresContext, aContent, aParent,
                             aContext, aPrevInFlow);

  Init();
  
  return rv;
}


NS_IMETHODIMP
nsSVGInnerSVGFrame::AppendFrames(nsPresContext* aPresContext,
                                 nsIPresShell&  aPresShell,
                                 nsIAtom*       aListName,
                                 nsIFrame*      aFrameList)
{
  // append == insert at end:
  return InsertFrames(aPresContext, aPresShell, aListName,
                      mFrames.LastChild(), aFrameList);  
}

NS_IMETHODIMP
nsSVGInnerSVGFrame::InsertFrames(nsPresContext* aPresContext,
                                 nsIPresShell&  aPresShell,
                                 nsIAtom*       aListName,
                                 nsIFrame*      aPrevFrame,
                                 nsIFrame*      aFrameList)
{
  nsIFrame* lastNewFrame = nsnull;
  {
    nsFrameList tmpList(aFrameList);
    lastNewFrame = tmpList.LastChild();
  }
  
  // Insert the new frames
  mFrames.InsertFrames(nsnull, aPrevFrame, aFrameList);

  // call InitialUpdate() on all new frames:
  nsIFrame* end = nsnull;
  if (lastNewFrame)
    end = lastNewFrame->GetNextSibling();

  for (nsIFrame*kid=aFrameList; kid != end; kid = kid->GetNextSibling()) {
    nsISVGChildFrame* SVGFrame=nsnull;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      SVGFrame->InitialUpdate(); 
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsSVGInnerSVGFrame::RemoveFrame(nsPresContext* aPresContext,
                                nsIPresShell&  aPresShell,
                                nsIAtom*       aListName,
                                nsIFrame*      aOldFrame)
{
  nsCOMPtr<nsISVGRendererRegion> dirty_region;
  
  nsISVGChildFrame* SVGFrame=nsnull;
  aOldFrame->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);

  if (SVGFrame)
    dirty_region = SVGFrame->GetCoveredRegion();

  PRBool result = mFrames.DestroyFrame(aPresContext, aOldFrame);

  nsISVGOuterSVGFrame* outerSVGFrame = GetOuterSVGFrame();
  NS_ASSERTION(outerSVGFrame, "no outer svg frame");
  if (dirty_region && outerSVGFrame)
    outerSVGFrame->InvalidateRegion(dirty_region, PR_TRUE);

  NS_ASSERTION(result, "didn't find frame to delete");
  return result ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSVGInnerSVGFrame::ReplaceFrame(nsPresContext* aPresContext,
                                 nsIPresShell&  aPresShell,
                                 nsIAtom*       aListName,
                                 nsIFrame*      aOldFrame,
                                 nsIFrame*      aNewFrame)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsSVGInnerSVGFrame::AttributeChanged(nsPresContext* aPresContext,
                                     nsIContent*    aChild,
                                     PRInt32        aNameSpaceID,
                                     nsIAtom*       aAttribute,
                                     PRInt32        aModType)
{
#ifdef DEBUG
    nsAutoString str;
    aAttribute->ToString(str);
    printf("** nsSVGInnerSVGFrame::AttributeChanged(%s)\n",
           NS_LossyConvertUCS2toASCII(str).get());
#endif

  return NS_OK;
}


//----------------------------------------------------------------------
// nsISVGChildFrame methods

NS_IMETHODIMP
nsSVGInnerSVGFrame::Paint(nsISVGRendererCanvas* canvas, const nsRect& dirtyRectTwips)
{
#ifdef DEBUG
//  printf("nsSVGInnerSVG(%p)::Paint\n", this);
#endif
  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame* SVGFrame=nsnull;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame)
      SVGFrame->Paint(canvas, dirtyRectTwips);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSVGInnerSVGFrame::GetFrameForPoint(float x, float y, nsIFrame** hit)
{
#ifdef DEBUG
//  printf("nsSVGInnerSVGFrame(%p)::GetFrameForPoint\n", this);
#endif
  *hit = nsnull;
  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame* SVGFrame=nsnull;
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
  }
  
  return *hit ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP_(already_AddRefed<nsISVGRendererRegion>)
nsSVGInnerSVGFrame::GetCoveredRegion()
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
nsSVGInnerSVGFrame::InitialUpdate()
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
  return NS_OK;
}  

NS_IMETHODIMP
nsSVGInnerSVGFrame::NotifyCanvasTMChanged()
{
  // make sure our cached transform matrix gets (lazily) updated
  mCanvasTM = nsnull;
  
  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame* SVGFrame=nsnull;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      SVGFrame->NotifyCanvasTMChanged();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGInnerSVGFrame::NotifyRedrawSuspended()
{
  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame* SVGFrame=0;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      SVGFrame->NotifyRedrawSuspended();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGInnerSVGFrame::NotifyRedrawUnsuspended()
{
  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame* SVGFrame=nsnull;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      SVGFrame->NotifyRedrawUnsuspended();
    }
  }
 return NS_OK;
}

NS_IMETHODIMP
nsSVGInnerSVGFrame::GetBBox(nsIDOMSVGRect **_retval)
{
  *_retval = nsnull;
  return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------
// nsISVGSVGFrame methods:

NS_IMETHODIMP
nsSVGInnerSVGFrame::SuspendRedraw()
{
  nsISVGOuterSVGFrame *outerSVGFrame = GetOuterSVGFrame();
  if (!outerSVGFrame) {
    NS_ERROR("no outer svg frame");
    return NS_ERROR_FAILURE;
  }
  return outerSVGFrame->SuspendRedraw();
}

NS_IMETHODIMP
nsSVGInnerSVGFrame::UnsuspendRedraw()
{
  nsISVGOuterSVGFrame *outerSVGFrame = GetOuterSVGFrame();
  if (!outerSVGFrame) {
    NS_ERROR("no outer svg frame");
    return NS_ERROR_FAILURE;
  }
  return outerSVGFrame->UnsuspendRedraw();
}

NS_IMETHODIMP
nsSVGInnerSVGFrame::NotifyViewportChange()
{
  // make sure canvas transform matrix gets (lazily) recalculated:
  mCanvasTM = nsnull;
  
  // inform children
  SuspendRedraw();
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGChildFrame* SVGFrame=nsnull;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame)
      SVGFrame->NotifyCanvasTMChanged(); 
    kid = kid->GetNextSibling();
  }
  UnsuspendRedraw();
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGContainerFrame methods:

nsISVGOuterSVGFrame *
nsSVGInnerSVGFrame::GetOuterSVGFrame()
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
nsSVGInnerSVGFrame::GetCanvasTM()
{
  // parentTM * move(x,y) * viewboxToViewportTM


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

    // viewbox to viewport:
    nsCOMPtr<nsIDOMSVGMatrix> viewBoxToViewportTM;
    nsCOMPtr<nsIDOMSVGSVGElement> svgElement = do_QueryInterface(mContent);
    NS_ASSERTION(svgElement, "wrong content element");
    svgElement->GetViewboxToViewportTransform(getter_AddRefs(viewBoxToViewportTM));

    parentTM->Multiply(viewBoxToViewportTM, getter_AddRefs(mCanvasTM));
  }    

  nsIDOMSVGMatrix* retval = mCanvasTM.get();
  NS_IF_ADDREF(retval);
  return retval;
}

already_AddRefed<nsSVGCoordCtxProvider>
nsSVGInnerSVGFrame::GetCoordContextProvider()
{
  NS_ASSERTION(mContent, "null parent");

  // Our <svg> content element is the CoordContextProvider for our children:
  nsSVGCoordCtxProvider *provider;
  mContent->QueryInterface(NS_GET_IID(nsSVGCoordCtxProvider), (void**)&provider);

  NS_IF_ADDREF(provider);
  
  return provider;  
}
