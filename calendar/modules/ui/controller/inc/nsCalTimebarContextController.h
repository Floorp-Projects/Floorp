/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsCalTimebarContextController_h___
#define nsCalTimebarContextController_h___

#include "nsCalContextController.h"

class nsCalTimebarContextController : public nsCalContextController
{
public:
  nsCalTimebarContextController(nsISupports * aOuter);

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init();

  // Subclass Canvas functions
  NS_IMETHOD_(nsEventStatus) PaintForeground(nsIRenderingContext& aRenderingContext,
                                             const nsRect& aDirtyRect);
  NS_IMETHOD_(nsEventStatus) OnLeftButtonDown(nsGUIEvent *aEvent);


  // nsIXMLParserObject methods
  NS_IMETHOD SetParameter(nsString& aKey, nsString& aValue) ;

  NS_IMETHOD GetClassPreferredSize(nsSize& aSize);

protected:
  ~nsCalTimebarContextController();

private:
  NS_METHOD GetTrianglePoints(nsPoint * pts);
  NS_METHOD_(PRBool) IsPointInTriangle(nsPoint aPoint, nsPoint * aTriangle);
  NS_METHOD RenderController(nsIRenderingContext& aCtx,
                             nsPoint* aPoints,
                             PRUint32 aNumPoints);


};

#endif /* nsCalTimebarContextController_h___ */
