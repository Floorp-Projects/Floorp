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
 * Scooter Morris.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scooter Morris <scootermorris@comcast.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsWeakReference.h"
#include "nsIDOMSVGStopElement.h"
#include "nsStyleContext.h"
#include "nsContainerFrame.h"
#include "nsLayoutAtoms.h"
#include "nsIDOMSVGStopElement.h"
#include "nsIDOMSVGAnimatedNumber.h"
#include "nsISVGValueObserver.h"
#include "nsISVGValueUtils.h"
#include "nsISVGValue.h"

// This is a very simple frame whose only purpose is to capture style change
// events and propogate them to the parent.  Most of the heavy lifting is done
// within the nsSVGGradientFrame, which is the parent for this frame

typedef nsContainerFrame  nsSVGStopFrameBase;

class nsSVGStopFrame : public nsSVGStopFrameBase,
                       public nsISVGValueObserver,
                       public nsSupportsWeakReference
{
  // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::svgStopFrame
   */
  virtual nsIAtom* GetType() const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGStop"), aResult);
  }
#endif

  NS_IMETHOD Init(nsPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsStyleContext*  aContext,
                  nsIFrame*        aPrevInFlow);

  // nsISVGValueObserver interface:
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable, 
                                     nsISVGValue::modificationType aModType);
  NS_IMETHOD DidModifySVGObservable(nsISVGValue* observable, 
                                    nsISVGValue::modificationType aModType);

protected:
  friend nsresult NS_NewSVGStopFrame(nsIPresShell* aPresShell, 
                                     nsIContent*   aContent, 
                                     nsIFrame*     aParentFrame, 
                                     nsIFrame**    aNewFrame);
  virtual ~nsSVGStopFrame();

private:
  // nsIFrame interface:
  NS_IMETHOD DidSetStyleContext(nsPresContext* aPresContext);

  nsCOMPtr<nsIDOMSVGAnimatedNumber> mOffset;
};

//----------------------------------------------------------------------
// nsISupports methods:

NS_INTERFACE_MAP_BEGIN(nsSVGStopFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
  NS_INTERFACE_MAP_ENTRY(nsSupportsWeakReference)
NS_INTERFACE_MAP_END_INHERITING(nsSVGStopFrameBase)

//----------------------------------------------------------------------
// Implementation

//----------------------------------------------------------------------
// nsIFrame methods:

NS_IMETHODIMP
nsSVGStopFrame::DidSetStyleContext(nsPresContext* aPresContext)
{
#ifdef DEBUG_scooter
  printf("nsSVGStopFrame::DidSetStyleContext\n");
#endif
  // Tell our parent
  if (mParent)
    mParent->DidSetStyleContext(aPresContext);
  return NS_OK;
}

nsIAtom *
nsSVGStopFrame::GetType() const
{
  return nsLayoutAtoms::svgStopFrame;
}

NS_IMETHODIMP
nsSVGStopFrame::Init(nsPresContext*  aPresContext,
                     nsIContent*     aContent,
                     nsIFrame*       aParent,
                     nsStyleContext* aContext,
                     nsIFrame*       aPrevInFlow)
{
  nsresult rv;
  rv = nsSVGStopFrameBase::Init(aPresContext, aContent, aParent,
                                aContext, aPrevInFlow);

  nsCOMPtr<nsIDOMSVGStopElement> stop = do_QueryInterface(mContent);
  {
    stop->GetOffset(getter_AddRefs(mOffset));
    if (!mOffset)
      return NS_ERROR_FAILURE;
    NS_ADD_SVGVALUE_OBSERVER(mOffset);
  }

  return rv;
}


//----------------------------------------------------------------------
// nsISVGValueObserver methods:
NS_IMETHODIMP
nsSVGStopFrame::WillModifySVGObservable(nsISVGValue* observable,
                                        nsISVGValue::modificationType aModType)
{
  // Need to tell our parent gradients that something happened.
  // Calling {Begin,End}Update on an nsISVGValue, which
  // nsSVGGradientFrame implements, causes its observers (the
  // referencing graphics frames) to be notified.
  if (mParent) {
    nsISVGValue *value;
    if (NS_SUCCEEDED(mParent->QueryInterface(NS_GET_IID(nsISVGValue),
                                             (void **)&value)))
      value->BeginBatchUpdate();
  }
  return NS_OK;
}
                                                                                
NS_IMETHODIMP
nsSVGStopFrame::DidModifySVGObservable(nsISVGValue* observable, 
                                       nsISVGValue::modificationType aModType)
{
  // Need to tell our parent gradients that something happened.
  // Calling {Begin,End}Update on an nsISVGValue, which
  // nsSVGGradientFrame implements, causes its observers (the
  // referencing graphics frames) to be notified.
  if (mParent) {
    nsISVGValue *value;
    if (NS_SUCCEEDED(mParent->QueryInterface(NS_GET_IID(nsISVGValue),
                                             (void **)&value)))
      value->EndBatchUpdate();
  }
  return NS_OK;
}


// -------------------------------------------------------------------------
// Public functions
// -------------------------------------------------------------------------

nsresult NS_NewSVGStopFrame(nsIPresShell* aPresShell, 
                            nsIContent*   aContent, 
                            nsIFrame*     aParentFrame, 
                            nsIFrame**    aNewFrame)
{
  *aNewFrame = nsnull;
  
#ifdef DEBUG_scooter
  printf("NS_NewSVGStopFrame\n");
#endif

  nsCOMPtr<nsIDOMSVGStopElement> grad = do_QueryInterface(aContent);
  NS_ASSERTION(grad, "NS_NewSVGStopFrame -- Content doesn't support nsIDOMSVGStopElement");
  if (!grad)
    return NS_ERROR_FAILURE;

  nsSVGStopFrame* it = new (aPresShell) nsSVGStopFrame;
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;

  return NS_OK;
}

nsSVGStopFrame::~nsSVGStopFrame()
{
  if (mOffset)
    NS_REMOVE_SVGVALUE_OBSERVER(mOffset);
}
