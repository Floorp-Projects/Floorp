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

//
// nsPolylineFrame
//

#ifndef nsPolylineFrame_h__
#define nsPolylineFrame_h__


#include "nsPolygonFrame.h"

class nsString;


nsresult NS_NewPolylineFrame(nsIPresShell* aPresShell, nsIFrame** aResult) ;


// XXX - !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// This should NOT be derived from nsLeafFrame
// we really want to create our own container class from the nsIFrame
// interface and not derive from any HTML Frames
// XXX - !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
class nsPolylineFrame : public nsPolygonFrame
{
  NS_IMETHOD RenderPoints(nsIRenderingContext& aRenderingContext,
                          const nsPoint aPoints[], PRInt32 aNumPoints);
   
}; // class nsPolylineFrame

#endif
