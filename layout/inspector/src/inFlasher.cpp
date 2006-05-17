/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "inFlasher.h"
#include "dsinfo.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIServiceManager.h"
#include "nsIFile.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIDocument.h"
#include "nsIDocShell.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIRenderingContext.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMElement.h"
#include "nsIView.h" 
#include "nsIViewManager.h" 
#include "nsIScriptGlobalObject.h"

///////////////////////////////////////////////////////////////////////////////

inFlasher::inFlasher()
{
  NS_INIT_REFCNT();
}

inFlasher::~inFlasher()
{
}

NS_IMPL_ISUPPORTS1(inFlasher, inIFlasher);

///////////////////////////////////////////////////////////////////////////////
// inIFlasher

NS_IMETHODIMP 
inFlasher::RepaintElement(nsIDOMElement* aElement, nsIDOMWindowInternal* aWindow)
{
  nsIPresShell* presShell = GetPresShellFor(aWindow);
  nsIFrame* frame = GetFrameFor(aElement, presShell);
  if (!frame) return NS_OK;

  nsCOMPtr<nsIPresContext> pcontext;
  presShell->GetPresContext(getter_AddRefs(pcontext));

  nsIFrame* parentWithView;
  frame->GetParentWithView(pcontext, &parentWithView);
  if (parentWithView) {
    nsIView* view;
    parentWithView->GetView(pcontext, &view);
    if (view) {
      nsIViewManager* viewManager;
      view->GetViewManager(viewManager);
      
      nsRect rect;
      parentWithView->GetRect(rect);

      viewManager->UpdateView(view, rect, NS_VMREFRESH_NO_SYNC);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP 
inFlasher::DrawElementOutline(nsIDOMElement* aElement, nsIDOMWindowInternal* aWindow, const PRUnichar* aColor, PRInt32 aThickness)
{
  nsIPresShell* presShell = GetPresShellFor(aWindow);
  nsIFrame* frame = GetFrameFor(aElement, presShell);
  if (!frame) return NS_OK;

  nsCOMPtr<nsIPresContext> pcontext;
  presShell->GetPresContext(getter_AddRefs(pcontext));
  nsCOMPtr<nsIRenderingContext> rcontext;
  presShell->CreateRenderingContext(frame, getter_AddRefs(rcontext));

  nsAutoString colorStr;
  colorStr.Assign(aColor);
  nscolor color;
  PRBool rv = NS_HexToRGB(colorStr, &color);

  nsRect rect;
  frame->GetRect(rect);
  nsPoint origin(0,0);
  GetClientOrigin(frame, origin);

  float p2t;
  pcontext->GetPixelsToTwips(&p2t);

  DrawOutline(origin.x, origin.y, rect.width, rect.height, color, aThickness, p2t, rcontext);

  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// inFlasher

NS_IMETHODIMP 
inFlasher::DrawOutline(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight, nscolor aColor, 
                              PRUint32 aThickness, float aP2T, nsIRenderingContext* aRenderContext)
{
  aRenderContext->SetColor(aColor);
  DrawLine(aX, aY, aWidth, aThickness, DIR_HORIZONTAL, BOUND_OUTER, aP2T, aRenderContext);
  DrawLine(aX, aY, aHeight, aThickness, DIR_VERTICAL, BOUND_OUTER, aP2T, aRenderContext);
  DrawLine(aX, aY+aHeight, aWidth, aThickness, DIR_HORIZONTAL, BOUND_INNER, aP2T, aRenderContext);
  DrawLine(aX+aWidth, aY, aHeight, aThickness, DIR_VERTICAL, BOUND_INNER, aP2T, aRenderContext);

  return NS_OK;
}

NS_IMETHODIMP 
inFlasher::DrawLine(nscoord aX, nscoord aY, nscoord aLength, PRUint32 aThickness, 
                           PRBool aDir, PRBool aBounds, float aP2T, nsIRenderingContext* aRenderContext)
{
  nscoord thickTwips = NSIntPixelsToTwips(aThickness, aP2T);
  if (aDir) { // horizontal
    aRenderContext->FillRect(aX, aY+(aBounds?0:-thickTwips), aLength, thickTwips);
  } else { // vertical
    aRenderContext->FillRect(aX+(aBounds?0:-thickTwips), aY, thickTwips, aLength);
  }

  return NS_OK;
}

NS_IMETHODIMP
inFlasher::GetClientOrigin(nsIFrame* aFrame, nsPoint& aPoint)
{
  nsIFrame* parent = aFrame;
  while (parent) {
    nsPoint origin;
    parent->GetOrigin(origin);
    aPoint.x += origin.x;
    aPoint.y += origin.y;
    parent->GetParent(&parent);
  }
  return NS_OK;
}

nsIFrame*
inFlasher::GetFrameFor(nsIDOMElement* aElement, nsIPresShell* aShell)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
  nsIFrame* frame;
  aShell->GetPrimaryFrameFor(content, &frame);
  return frame;
}

nsIPresShell*
inFlasher::GetPresShellFor(nsISupports* aThing)
{
  nsCOMPtr<nsIScriptGlobalObject> so = do_QueryInterface(aThing);
  nsCOMPtr<nsIDocShell> docShell;
  so->GetDocShell(getter_AddRefs(docShell));
  
  nsIPresShell* presShell;
  docShell->GetPresShell(&presShell);

  return presShell;
}
