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

#ifndef __inFlasher_h__
#define __inFlasher_h__

#include "inIFlasher.h"

#include "nsIDOMElement.h"
#include "nsIPresShell.h"
#include "nsIFrame.h"
#include "nsIRenderingContext.h"

#define BOUND_INNER 0
#define BOUND_OUTER 1

#define DIR_VERTICAL 0
#define DIR_HORIZONTAL 1 

class inFlasher : public inIFlasher
{
public:
  inFlasher();
  ~inFlasher();

protected:
  nsIFrame* GetFrameFor(nsIDOMElement* aElement, nsIPresShell* aShell);
  nsIPresShell* GetPresShellFor(nsISupports* aThing);

  NS_IMETHOD DrawOutline(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight, nscolor aColor, 
                           PRUint32 aThickness, float aP2T, nsIRenderingContext* aRenderContext);
  NS_IMETHOD DrawLine(nscoord aX, nscoord aY, nscoord aLength, PRUint32 aThickness, 
                        PRBool aDir, PRBool aBounds, float aP2T, nsIRenderingContext* aRenderContext);
  NS_IMETHOD GetClientOrigin(nsIFrame* aFrame, nsPoint& aPoint);
  
public:
  NS_DECL_ISUPPORTS

  NS_DECL_INIFLASHER
};

//////////////////////////////////////////////////////////////

// {9286E71A-621A-4b91-851E-9984C1A2E81A}
#define IN_FLASHER_CID \
{ 0x9286e71a, 0x621a, 0x4b91, { 0x85, 0x1e, 0x99, 0x84, 0xc1, 0xa2, 0xe8, 0x1a } }

#define IN_FLASHER_CONTRACTID \
"@mozilla.org/inspector/flasher;1"

#endif
