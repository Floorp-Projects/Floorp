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


#include "nsSVGPathFrame.h"

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
//#include "nsSVGPathCID.h"
#include "nsReadableUtils.h"

//
// NS_NewSVGPathFrame
//
// Wrapper for creating a new color picker
//
nsresult
NS_NewSVGPathFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsSVGPathFrame* it = new (aPresShell) nsSVGPathFrame;
  if ( !it )
    return NS_ERROR_OUT_OF_MEMORY;
  *aNewFrame = it;
  return NS_OK;
}

// static NS_DEFINE_IID(kDefSVGPathCID, NS_DEFCOLORPICKER_CID);

//
// nsSVGPathFrame cntr
//
nsSVGPathFrame::nsSVGPathFrame() :
  mPath(nsnull),
  mX(0),
  mY(0)
{

}

nsSVGPathFrame::~nsSVGPathFrame()
{
//  if (mPath) {
//    delete mPath;
//  }
}


NS_IMETHODIMP
nsSVGPathFrame::Init(nsIPresContext*  aPresContext,
                         nsIContent*      aContent,
                         nsIFrame*        aParent,
                         nsIStyleContext* aContext,
                         nsIFrame*        aPrevInFlow)
{
 
  nsresult rv = nsLeafFrame::Init(aPresContext, aContent, aParent, aContext,
                                  aPrevInFlow);


  nsAutoString type;
  mContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::type, type);

  if (type.EqualsIgnoreCase(NS_ConvertASCIItoUCS2("swatch")) || type.IsEmpty())
  {
    //mSVGPath = new nsStdSVGPath();
    //mSVGPath->Init(mContent);
  }

  return rv;
}

//--------------------------------------------------------------
// Frames are not refcounted, no need to AddRef
NS_IMETHODIMP
nsSVGPathFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
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
//-- Main Reflow for the SVGPath
//-------------------------------------------------------------------
NS_IMETHODIMP 
nsSVGPathFrame::Reflow(nsIPresContext*          aPresContext, 
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
    char * s = ToNewCString(coordStr);
    mX = NSIntPixelsToTwips(atoi(s), p2t*scale);
    delete [] s;
  }

  res = mContent->GetAttr(kNameSpaceID_None, nsSVGAtoms::y, coordStr);
  if (NS_SUCCEEDED(res)) {
    char * s = ToNewCString(coordStr);
    mY = NSIntPixelsToTwips(atoi(s), p2t*scale);
    delete [] s;
  }

  nsAutoString pathStr;
  res = mContent->GetAttr(kNameSpaceID_None, nsSVGAtoms::d, pathStr);
  if (NS_SUCCEEDED(res)) {
    char * s = ToNewCString(pathStr);
    // parse path commands here
    delete [] s;
  }

  // Create Path object here;
  //mPath = new nsPathObject();
  //
  // iterate thru path commands here and load up path
  // calculate the rect the the path owns

  //aDesiredSize.width  = maxWidth  + nscoord(p2t*scale);
  //aDesiredSize.height = maxHeight + nscoord(p2t*scale);
  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;

  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width  = aDesiredSize.width;
    aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGPathFrame::HandleEvent(nsIPresContext* aPresContext, 
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
nsSVGPathFrame::HandleMouseDownEvent(nsIPresContext* aPresContext, 
                                         nsGUIEvent*     aEvent,
                                         nsEventStatus*  aEventStatus)
{

  return NS_OK;
}

NS_IMETHODIMP nsSVGPathFrame::SetProperty(nsIPresContext* aPresContext, 
                                          nsIAtom* aName, 
                                          const nsString& aValue)
{
  if (aName == nsSVGAtoms::d) {
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGPathFrame::AttributeChanged(nsIPresContext* aPresContext,
                                          nsIContent*     aChild,
                                          PRInt32         aNameSpaceID,
                                          nsIAtom*        aAttribute,
                                          PRInt32         aModType, 
                                          PRInt32         aHint)
{
  return nsLeafFrame::AttributeChanged(aPresContext, aChild, aNameSpaceID, aAttribute, aModType, aHint);
}
  

//
// Paint
//
//
NS_METHOD 
nsSVGPathFrame::Paint(nsIPresContext*          aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          const nsRect&        aDirtyRect,
                          nsFramePaintLayer    aWhichLayer,
                          PRUint32             aFlags)
{
  const nsStyleVisibility* visib = NS_STATIC_CAST(const nsStyleVisibility*,
    mStyleContext->GetStyleData(eStyleStruct_Visibility));

  // if we aren't visible then we are done.
  if (!visib->IsVisibleOrCollapsed()) 
	   return NS_OK;  

  // if we are visible then tell our superclass to paint
  nsLeafFrame::Paint(aPresContext, aRenderingContext, aDirtyRect,
                     aWhichLayer);

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
  aRenderingContext.SetClipRect(rect, nsClipCombine_kReplace, clipState);
  aRenderingContext.SetColor(color);
  
  ///////////////////////////////////////////
  // XXX - This is all just a quick hack
  // needs to be rewritten
  nsCOMPtr<nsIDeviceContext> dx;
  aPresContext->GetDeviceContext(getter_AddRefs(dx));
  float p2t   = 1.0;
  float scale = 1.0;
  if (dx) {
    aPresContext->GetPixelsToTwips(&p2t);
    dx->GetCanonicalPixelScale(scale); 
  }  
  
  // render path here

  aRenderingContext.PopState(clipState);

  return NS_OK;
}


//
// GetDesiredSize
//
// For now, be as big as CSS wants us to be, or some small default size.
//
void
nsSVGPathFrame::GetDesiredSize(nsIPresContext* aPresContext,
                               const nsHTMLReflowState& aReflowState,
                               nsHTMLReflowMetrics& aDesiredSize)
{
  NS_ASSERTION(0, "Who called this? and Why?");
} // GetDesiredSize
