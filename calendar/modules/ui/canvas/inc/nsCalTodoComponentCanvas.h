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

#ifndef nsCalTodoComponentCanvas_h___
#define nsCalTodoComponentCanvas_h___

#include "nsCalTimebarComponentCanvas.h"
#include "nsIWidget.h"
#include "nsIListWidget.h"

class nsCalTodoComponentCanvas : public nsCalTimebarComponentCanvas
{

public:
  nsCalTodoComponentCanvas(nsISupports* outer);

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init();
  NS_IMETHOD SetBounds(const nsRect& aBounds);
  NS_IMETHOD_(void)    SetBackgroundColor(const nscolor &aColor) ;
  NS_IMETHOD_(nsEventStatus) OnPaint(nsIRenderingContext& aRenderingContext,
                                     const nsRect& aDirtyRect);
  NS_IMETHOD_(nsEventStatus) HandleEvent(nsGUIEvent *aEvent);

protected:
  ~nsCalTodoComponentCanvas();

};

#endif /* nsCalTodoComponentCanvas_h___ */
