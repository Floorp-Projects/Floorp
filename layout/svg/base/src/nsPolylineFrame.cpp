/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


#include "nsPolylineFrame.h"

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
//#include "nsPolylineCID.h"
//
// NS_NewPolylineFrame
//
// Wrapper for creating a new color picker
//
nsresult
NS_NewPolylineFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsPolylineFrame* it = new (aPresShell) nsPolylineFrame;
  if ( !it )
    return NS_ERROR_OUT_OF_MEMORY;
  *aNewFrame = it;
  return NS_OK;
}

NS_METHOD nsPolylineFrame::RenderPoints(nsIRenderingContext& aRenderingContext,
                                        const nsPoint aPoints[], PRInt32 aNumPoints)
{
  aRenderingContext.DrawPolyline(aPoints, aNumPoints-1);
  return NS_OK;
}
