/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
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
 *    Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "nsContainerFrame.h"
#include "nsIDOMSVGGElement.h"
#include "nsIPresContext.h"
#include "nsISVGFrame.h"
#include "nsSVGRenderingContext.h"
#include "nsWeakReference.h"
#include "nsISVGValue.h"
#include "nsISVGValueObserver.h"
#include "nsIDOMSVGTransformable.h"
#include "nsIDOMSVGAnimTransformList.h"

typedef nsContainerFrame nsSVGGFrameBase;

class nsSVGGFrame : public nsSVGGFrameBase,
                    public nsISVGFrame,
                    public nsISVGValueObserver,
                    public nsSupportsWeakReference
{
  friend nsresult
  NS_NewSVGGFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame);
protected:
  nsSVGGFrame();
  virtual ~nsSVGGFrame();
  nsresult Init();
  
   // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }  
public:
  // nsIFrame:

  NS_IMETHOD  AppendFrames(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aFrameList);
  NS_IMETHOD  InsertFrames(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aPrevFrame,
                           nsIFrame*       aFrameList);
  NS_IMETHOD  RemoveFrame(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aOldFrame);
  NS_IMETHOD  ReplaceFrame(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aOldFrame,
                           nsIFrame*       aNewFrame);
  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsStyleContext*  aContext,
                  nsIFrame*        aPrevInFlow);


  // nsISVGValueObserver
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable);
  NS_IMETHOD DidModifySVGObservable (nsISVGValue* observable);

  // nsISupportsWeakReference
  // implementation inherited from nsSupportsWeakReference
  
  // nsISVGFrame interface:
  NS_IMETHOD Paint(nsSVGRenderingContext* renderingContext);
  NS_IMETHOD InvalidateRegion(ArtUta* uta, PRBool bRedraw);
  NS_IMETHOD GetFrameForPoint(float x, float y, nsIFrame** hit);  
  NS_IMETHOD NotifyCTMChanged();
  NS_IMETHOD NotifyRedrawSuspended();
  NS_IMETHOD NotifyRedrawUnsuspended();
  NS_IMETHOD IsRedrawSuspended(PRBool* isSuspended);

protected:
};

//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGGFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame)
{
  *aNewFrame = nsnull;
  
  nsCOMPtr<nsIDOMSVGTransformable> transformable = do_QueryInterface(aContent);
  if (!transformable) {
#ifdef DEBUG
    printf("warning: trying to construct an SVGGFrame for a content element that doesn't support the right interfaces\n");
#endif
    return NS_ERROR_FAILURE;
  }
  
  nsSVGGFrame* it = new (aPresShell) nsSVGGFrame;
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;

  return NS_OK;
}

nsSVGGFrame::nsSVGGFrame()
{
}

nsSVGGFrame::~nsSVGGFrame()
{
  nsCOMPtr<nsIDOMSVGTransformable> transformable = do_QueryInterface(mContent);
  NS_ASSERTION(transformable, "wrong content element");
  nsCOMPtr<nsIDOMSVGAnimatedTransformList> transforms;
  transformable->GetTransform(getter_AddRefs(transforms));
  nsCOMPtr<nsISVGValue> value = do_QueryInterface(transforms);
  NS_ASSERTION(value, "interface not found");
  if (value)
    value->RemoveObserver(this);
}

nsresult nsSVGGFrame::Init()
{
  nsCOMPtr<nsIDOMSVGTransformable> transformable = do_QueryInterface(mContent);
  NS_ASSERTION(transformable, "wrong content element");
  nsCOMPtr<nsIDOMSVGAnimatedTransformList> transforms;
  transformable->GetTransform(getter_AddRefs(transforms));
  nsCOMPtr<nsISVGValue> value = do_QueryInterface(transforms);
  NS_ASSERTION(value, "interface not found");
  if (value)
    value->AddObserver(this);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN(nsSVGGFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGFrame)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
NS_INTERFACE_MAP_END_INHERITING(nsSVGGFrameBase)


//----------------------------------------------------------------------
// nsIFrame methods
NS_IMETHODIMP
nsSVGGFrame::Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsStyleContext*  aContext,
                  nsIFrame*        aPrevInFlow)
{
  nsresult rv;
  rv = nsSVGGFrameBase::Init(aPresContext, aContent, aParent,
                             aContext, aPrevInFlow);

  Init();
  
  return rv;
}

NS_IMETHODIMP
nsSVGGFrame::AppendFrames(nsIPresContext* aPresContext,
                      nsIPresShell&   aPresShell,
                      nsIAtom*        aListName,
                      nsIFrame*       aFrameList)
{
  nsresult  rv = NS_OK;

  // Insert the new frames
  mFrames.AppendFrames(this, aFrameList);

  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGFrame* SVGFrame=0;
    kid->QueryInterface(NS_GET_IID(nsISVGFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      SVGFrame->NotifyCTMChanged(); //XXX use different function
    }
  }

  return rv;
}

NS_IMETHODIMP
nsSVGGFrame::InsertFrames(nsIPresContext* aPresContext,
                      nsIPresShell&   aPresShell,
                      nsIAtom*        aListName,
                      nsIFrame*       aPrevFrame,
                      nsIFrame*       aFrameList)
{
  nsresult  rv = NS_OK;
  
  // Insert the new frames
#ifdef NS_DEBUG
  nsFrame::VerifyDirtyBitSet(aFrameList);
#endif
  mFrames.InsertFrames(nsnull, aPrevFrame, aFrameList);

  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGFrame* SVGFrame=0;
    kid->QueryInterface(NS_GET_IID(nsISVGFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      SVGFrame->NotifyCTMChanged(); //XXX use different function
    }
  }
  
  return rv;
}

NS_IMETHODIMP
nsSVGGFrame::RemoveFrame(nsIPresContext* aPresContext,
                     nsIPresShell&   aPresShell,
                     nsIAtom*        aListName,
                     nsIFrame*       aOldFrame)
{
  PRBool result = mFrames.DestroyFrame(aPresContext, aOldFrame);
  NS_ASSERTION(result, "didn't find frame to delete");
  // Because positioned frames aren't part of a flow, there's no
  // additional work to do, e.g. reflowing sibling frames. And because
  // positioned frames have a view, we don't need to repaint
  return result ? NS_OK : NS_ERROR_FAILURE;

}

NS_IMETHODIMP
nsSVGGFrame::ReplaceFrame(nsIPresContext* aPresContext,
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
nsSVGGFrame::WillModifySVGObservable(nsISVGValue* observable)
{
  return NS_OK;
}


NS_IMETHODIMP
nsSVGGFrame::DidModifySVGObservable (nsISVGValue* observable)
{
  
  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGFrame* SVGFrame=0;
    kid->QueryInterface(NS_GET_IID(nsISVGFrame),(void**)&SVGFrame);
    if (SVGFrame)
      SVGFrame->NotifyCTMChanged();
  }  
  return NS_OK;
}


//----------------------------------------------------------------------
// nsISVGFrame methods

NS_IMETHODIMP
nsSVGGFrame::Paint(nsSVGRenderingContext* renderingContext)
{
  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGFrame* SVGFrame=0;
    kid->QueryInterface(NS_GET_IID(nsISVGFrame),(void**)&SVGFrame);
    if (SVGFrame)
      SVGFrame->Paint(renderingContext);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSVGGFrame::InvalidateRegion(ArtUta* uta, PRBool bRedraw)
{
  if (!uta && !bRedraw) return NS_OK;
  
  if (!mParent) {
    if (uta)
      art_uta_free(uta);
    return NS_OK;
  }

  nsCOMPtr<nsISVGFrame> SVGFrame = do_QueryInterface(mParent);
  if (!SVGFrame) {
    if (uta)
      art_uta_free(uta);
    return NS_OK;
  }

  return SVGFrame->InvalidateRegion(uta, bRedraw);
}

NS_IMETHODIMP
nsSVGGFrame::GetFrameForPoint(float x, float y, nsIFrame** hit)
{
  *hit = nsnull;
  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGFrame* SVGFrame=0;
    kid->QueryInterface(NS_GET_IID(nsISVGFrame),(void**)&SVGFrame);
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

NS_IMETHODIMP
nsSVGGFrame::NotifyCTMChanged()
{
  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGFrame* SVGFrame=0;
    kid->QueryInterface(NS_GET_IID(nsISVGFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      SVGFrame->NotifyCTMChanged();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGGFrame::NotifyRedrawSuspended()
{
  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGFrame* SVGFrame=0;
    kid->QueryInterface(NS_GET_IID(nsISVGFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      SVGFrame->NotifyRedrawSuspended();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGGFrame::NotifyRedrawUnsuspended()
{
  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGFrame* SVGFrame=0;
    kid->QueryInterface(NS_GET_IID(nsISVGFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      SVGFrame->NotifyRedrawUnsuspended();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGGFrame::IsRedrawSuspended(PRBool* isSuspended)
{
  nsCOMPtr<nsISVGFrame> SVGFrame = do_QueryInterface(mParent);
  if (!SVGFrame) {
    *isSuspended = PR_FALSE;
    return NS_OK;
  }

  return SVGFrame->IsRedrawSuspended(isSuspended);  
}

