/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 *
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
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include "inLayoutUtils.h"

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
inFlasher::RepaintElement(nsIDOMElement* aElement)
{
  nsCOMPtr<nsIDOMWindowInternal> window = inLayoutUtils::GetWindowFor(aElement);
  if (!window) return NS_OK;
  nsCOMPtr<nsIPresShell> presShell = inLayoutUtils::GetPresShellFor(window);
  if (!presShell) return NS_OK;
  nsIFrame* frame = inLayoutUtils::GetFrameFor(aElement, presShell);
  if (!frame) return NS_OK;

  nsCOMPtr<nsIPresContext> pcontext;
  presShell->GetPresContext(getter_AddRefs(pcontext));

  nsIFrame* parentWithView;
  frame->GetParentWithView(pcontext, &parentWithView);
  if (parentWithView) {
    nsIView* view;
    parentWithView->GetView(pcontext, &view);
    if (view) {
      nsCOMPtr<nsIViewManager> viewManager;
      view->GetViewManager(*getter_AddRefs(viewManager));
      if (viewManager) {
        nsRect rect;
        parentWithView->GetRect(rect);

        viewManager->UpdateView(view, rect, NS_VMREFRESH_NO_SYNC);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP 
inFlasher::DrawElementOutline(nsIDOMElement* aElement, const PRUnichar* aColor, PRInt32 aThickness)
{
  nsCOMPtr<nsIDOMWindowInternal> window = inLayoutUtils::GetWindowFor(aElement);
  if (!window) return NS_OK;
  nsCOMPtr<nsIPresShell> presShell = inLayoutUtils::GetPresShellFor(window);
  nsIFrame* frame = inLayoutUtils::GetFrameFor(aElement, presShell);
  if (!frame) return NS_OK;

  nsCOMPtr<nsIPresContext> pcontext;
  presShell->GetPresContext(getter_AddRefs(pcontext));
  nsCOMPtr<nsIRenderingContext> rcontext;
  presShell->CreateRenderingContext(frame, getter_AddRefs(rcontext));

  nsAutoString colorStr;
  colorStr.Assign(aColor);
  nscolor color;
  NS_HexToRGB(colorStr, &color);

  nsRect rect;
  frame->GetRect(rect);
  nsPoint origin = inLayoutUtils::GetClientOrigin(frame);
  rect.x = origin.x;
  rect.y = origin.y;
  inLayoutUtils::AdjustRectForMargins(aElement, rect);
  
  float p2t;
  pcontext->GetPixelsToTwips(&p2t);

  DrawOutline(rect.x, rect.y, rect.width, rect.height, color, aThickness, p2t, rcontext);

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
