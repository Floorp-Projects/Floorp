/*
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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Joe Hewitt <hewitt@netscape.com> (original author)
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
  NS_DECL_ISUPPORTS
  NS_DECL_INIFLASHER

  inFlasher();
  virtual ~inFlasher();

protected:
  nsIFrame* GetFrameFor(nsIDOMElement* aElement, nsIPresShell* aShell);
  nsIPresShell* GetPresShellFor(nsISupports* aThing);

  NS_IMETHOD DrawOutline(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight, nscolor aColor, 
                           PRUint32 aThickness, float aP2T, nsIRenderingContext* aRenderContext);
  NS_IMETHOD DrawLine(nscoord aX, nscoord aY, nscoord aLength, PRUint32 aThickness, 
                        PRBool aDir, PRBool aBounds, float aP2T, nsIRenderingContext* aRenderContext);
};

#endif // __inFlasher_h__
