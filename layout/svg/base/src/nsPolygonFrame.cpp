/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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


#include "nsPolygonFrame.h"

#include "nsIDOMElement.h"
#include "nsIContent.h"
#include "prtypes.h"
#include "nsIAtom.h"
#include "nsHTMLAtoms.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsCSSRendering.h"
#include "nsINameSpaceManager.h"
#include "nsColor.h"
#include "nsIServiceManager.h"
#include "nsPoint.h"
#include "nsSVGAtoms.h"
#include "nsIDeviceContext.h"
#include "nsGUIEvent.h"

#include "nsIReflowCommand.h"
extern nsresult
NS_NewHTMLReflowCommand(nsIReflowCommand**           aInstancePtrResult,
                        nsIFrame*                    aTargetFrame,
                        nsIReflowCommand::ReflowType aReflowType,
                        nsIFrame*                    aChildFrame = nsnull,
                        nsIAtom*                     aAttribute = nsnull);
//#include "nsPolygonCID.h"
//
// NS_NewPolygonFrame
//
// Wrapper for creating a new color picker
//
nsresult
NS_NewPolygonFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsPolygonFrame* it = new (aPresShell) nsPolygonFrame;
  if ( !it )
    return NS_ERROR_OUT_OF_MEMORY;
  *aNewFrame = it;
  return NS_OK;
}

// static NS_DEFINE_IID(kDefPolygonCID, NS_DEFCOLORPICKER_CID);

//
// nsPolygonFrame cntr
//
nsPolygonFrame::nsPolygonFrame() :
  mPnts(nsnull),
  mNumPnts(0),
  mX(0),
  mY(0),
  mPresContext(nsnull)
{

}

nsPolygonFrame::~nsPolygonFrame()
{
  if (mPnts) {
    delete [] mPnts;
  }
  NS_IF_RELEASE(mPresContext);
}


NS_IMETHODIMP
nsPolygonFrame::Init(nsIPresContext*  aPresContext,
                         nsIContent*      aContent,
                         nsIFrame*        aParent,
                         nsIStyleContext* aContext,
                         nsIFrame*        aPrevInFlow)
{
  mPresContext = aPresContext;
  NS_ADDREF(mPresContext);
 
  nsresult rv = nsLeafFrame::Init(aPresContext, aContent, aParent, aContext,
                                  aPrevInFlow);


  nsAutoString type;
  mContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::type, type);

  if (type.EqualsIgnoreCase(NS_ConvertASCIItoUCS2("swatch")) || type.IsEmpty())
  {
    //mPolygon = new nsStdPolygon();
    //mPolygon->Init(mContent);
  }

  return rv;
}

//--------------------------------------------------------------
// Frames are not refcounted, no need to AddRef
NS_IMETHODIMP
nsPolygonFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aIID.Equals(NS_GET_IID(nsISVGFrame))) {
    *aInstancePtr = (void*) ((nsISVGFrame*) this);
    return NS_OK;
  }
  return nsLeafFrame::QueryInterface(aIID, aInstancePtr);
}


//-------------------------------------------------------------------
//-- Main Reflow for the Polygon
//-------------------------------------------------------------------
NS_IMETHODIMP 
nsPolygonFrame::Reflow(nsIPresContext*          aPresContext, 
                       nsHTMLReflowMetrics&     aDesiredSize,
                       const nsHTMLReflowState& aReflowState, 
                       nsReflowStatus&          aStatus)
{
  aStatus = NS_FRAME_COMPLETE;

  nsCOMPtr<nsIDeviceContext> dx;
  aPresContext->GetDeviceContext(getter_AddRefs(dx));
  float p2t   = 1.0;
  float scale = 1.0;
  if (dx) { 
    aPresContext->GetPixelsToTwips(&p2t);
    dx->GetCanonicalPixelScale(scale); 
  }  
  
  nsAutoString coordStr;
  nsresult res = mContent->GetAttr(kNameSpaceID_None, nsSVGAtoms::x, coordStr);
  if (NS_SUCCEEDED(res)) {
    char * s = coordStr.ToNewCString();
    mX = NSIntPixelsToTwips(atoi(s), p2t*scale);
    delete [] s;
  }

  res = mContent->GetAttr(kNameSpaceID_None, nsSVGAtoms::y, coordStr);
  if (NS_SUCCEEDED(res)) {
    char * s = coordStr.ToNewCString();
    mY = NSIntPixelsToTwips(atoi(s), p2t*scale);
    delete [] s;
  }

  if (mPoints.Count() == 0) {
    GetPoints();
    // Convert points from Pixels to twips
    // and these points are relative to the X & Y
    // then find max width and height
    nscoord maxWidth  = 0;
    nscoord maxHeight = 0;
    for (PRInt32 i=0;i<mNumPnts;i++) {
      mPnts[i].x = NSIntPixelsToTwips(mPnts[i].x, p2t*scale);
      mPnts[i].y = NSIntPixelsToTwips(mPnts[i].y, p2t*scale);
      maxWidth   = PR_MAX(maxWidth,  mPnts[i].x);
      maxHeight  = PR_MAX(maxHeight, mPnts[i].y);
    }
    aDesiredSize.width  = maxWidth  + nscoord(p2t*scale);
    aDesiredSize.height = maxHeight + nscoord(p2t*scale);
  } else {
    aDesiredSize.width  = mRect.width;
    aDesiredSize.height = mRect.height;
  }

  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;

  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width  = aDesiredSize.width;
    aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }
  //nsRect  rect(0, 0, aDesiredSize.width, aDesiredSize.height);
  //Invalidate(aPresContext, rect, PR_TRUE);
  return NS_OK;
}

NS_IMETHODIMP
nsPolygonFrame::HandleEvent(nsIPresContext* aPresContext, 
                                nsGUIEvent*     aEvent,
                                nsEventStatus*  aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  *aEventStatus = nsEventStatus_eConsumeDoDefault;
	if (aEvent->message == NS_MOUSE_LEFT_BUTTON_DOWN)
		HandleMouseDownEvent(aPresContext, aEvent, aEventStatus);

  return NS_OK;
}

nsresult
nsPolygonFrame::HandleMouseDownEvent(nsIPresContext* aPresContext, 
                                         nsGUIEvent*     aEvent,
                                         nsEventStatus*  aEventStatus)
{

  return NS_OK;
}

// XXX - Quick and Dirty impl to get somethinf working
// this should be rewritten
NS_METHOD 
nsPolygonFrame::GetPoints()
{
  if (mPnts != nsnull) {
    delete [] mPnts;
    mNumPnts = 0;
    mPoints.Clear();
  }

  nsAutoString pointsStr;
  nsresult res = mContent->GetAttr(kNameSpaceID_None, nsSVGAtoms::points, pointsStr);

  char * ps = pointsStr.ToNewCString();
  char seps[]   = " ";
  char *token  = strtok(ps, seps);
  PRInt32 cnt = 0;
  nsPoint * pnt = nsnull;
  while (token != NULL) {
    if (cnt % 2 == 0) {
      pnt = new nsPoint;
      mPoints.AppendElement((void*)pnt);
      pnt->x = atoi(token);
    } else {
      pnt->y = atoi(token);
    }
    token = strtok( NULL, seps );
    cnt++;
  }

  delete [] ps;

  mNumPnts = mPoints.Count()+1;
  mPnts    = new nsPoint[mNumPnts];
  for (cnt=0;cnt<mNumPnts-1;cnt++) {
    pnt = NS_REINTERPRET_CAST(nsPoint*, mPoints.ElementAt(cnt));
    mPnts[cnt] = *pnt;
    delete pnt;
  }
  mPnts[mNumPnts-1] = mPnts[0];

  //printf("0x%X Points are:\n", this);
  //for (PRInt32 i=0;i<mNumPnts;i++) {
  //  printf("%d -> [%d,%d]\n", i, mPnts[i].x, mPnts[i].y);
  //}

  return NS_OK;
}

NS_IMETHODIMP nsPolygonFrame::SetProperty(nsIPresContext* aPresContext, 
                                          nsIAtom* aName, 
                                          const nsString& aValue)
{
  if (aName == nsSVGAtoms::points) {
  } else if (aName == nsSVGAtoms::x) {
  } else if (aName == nsSVGAtoms::y) {
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPolygonFrame::AttributeChanged(nsIPresContext* aPresContext,
                                          nsIContent*     aChild,
                                          PRInt32         aNameSpaceID,
                                          nsIAtom*        aAttribute,
                                          PRInt32         aModType, 
                                          PRInt32         aHint)
{
  if (aAttribute == nsSVGAtoms::points) {
    //GetPoints();
    mPoints.Clear();

    nsCOMPtr<nsIPresShell> shell;
    aPresContext->GetShell(getter_AddRefs(shell));
    
    nsIReflowCommand* reflowCmd;
    nsresult rv = NS_NewHTMLReflowCommand(&reflowCmd, this,
                                          nsIReflowCommand::ContentChanged);
    if (NS_SUCCEEDED(rv)) {
      shell->AppendReflowCommand(reflowCmd);
      NS_RELEASE(reflowCmd);
      rv = shell->FlushPendingNotifications(PR_FALSE);
    }
  } else if (aAttribute == nsSVGAtoms::x) {
  } else if (aAttribute == nsSVGAtoms::y) {
  }
  return nsLeafFrame::AttributeChanged(aPresContext, aChild, aNameSpaceID, aAttribute, aModType, aHint);
}
  
NS_METHOD nsPolygonFrame::RenderPoints(nsIRenderingContext& aRenderingContext,
                                       const nsPoint aPoints[], PRInt32 aNumPoints)
{
  //nsAutoString fillStr;
  //nsresult res = mContent->GetAttr(kNameSpaceID_None, nsSVGAtoms::fill, fillStr);
  //if (NS_SUCCEEDED(res)) {
    aRenderingContext.FillPolygon(aPoints, aNumPoints);
  //} else {
  //  aRenderingContext.DrawPolygon(aPoints, aNumPoints);
  //}
  return NS_OK;
}

//
// Paint
//
//
NS_METHOD 
nsPolygonFrame::Paint(nsIPresContext*          aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          const nsRect&        aDirtyRect,
                          nsFramePaintLayer    aWhichLayer,
                          PRUint32             aFlags)
{

  // if we aren't visible then we are done.
  const nsStyleVisibility* visib = NS_STATIC_CAST(const nsStyleVisibility*,
    mStyleContext->GetStyleData(eStyleStruct_Visibility));
  if (!visib->IsVisibleOrCollapsed()) 
	   return NS_OK;  

  if (aWhichLayer == NS_FRAME_PAINT_LAYER_BACKGROUND) {
    // if we are visible then tell our superclass to paint
    nsLeafFrame::Paint(aPresContext, aRenderingContext, aDirtyRect,
                       aWhichLayer);
  }

  if (aWhichLayer == NS_FRAME_PAINT_LAYER_FOREGROUND) {

    // get our border
	  const nsStyleBorder* borderStyle = (const nsStyleBorder*)mStyleContext->GetStyleData(eStyleStruct_Border);
	  nsMargin border(0,0,0,0);
	  borderStyle->CalcBorderFor(this, border);

  
    // XXX - Color needs to comes from new style property fill
    // and not mColor
    const nsStyleColor* colorStyle = (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
    nscolor color = colorStyle->mColor;
  

    aRenderingContext.PushState();

    // set the clip region
    nsRect rect;

    PRBool clipState;
    GetRect(rect);

    // Clip so we don't render outside the inner rect
    rect.x = 0;
    rect.y = 0;
    aRenderingContext.SetClipRect(rect, nsClipCombine_kReplace, clipState);
    aRenderingContext.SetColor(color);
  
    ///////////////////////////////////////////
    // XXX - This is all just a quick hack
    // needs to be rewritten
    nsPoint points[256];
    for (PRInt32 i=0;i<mNumPnts;i++) {
      points[i] = mPnts[i];
    }
    // XXX - down to here

    RenderPoints(aRenderingContext, points, mNumPnts);

    aRenderingContext.PopState(clipState);
  }

  return NS_OK;
}


//
// GetDesiredSize
//
// For now, be as big as CSS wants us to be, or some small default size.
//
void
nsPolygonFrame::GetDesiredSize(nsIPresContext* aPresContext,
                               const nsHTMLReflowState& aReflowState,
                               nsHTMLReflowMetrics& aDesiredSize)
{
  NS_ASSERTION(0, "Who called this? and Why?");
} // GetDesiredSize
