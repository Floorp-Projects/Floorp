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

#ifndef nsCalTimebarCanvas_h___
#define nsCalTimebarCanvas_h___

#define INSET 2

#include "nsXPFCCanvas.h"
#include "nsICalTimeContext.h"
#include "nsCalCanvas.h"
#include "nsDateTime.h"

class nsCalTimebarCanvas : public nsCalCanvas

{
public:
  nsCalTimebarCanvas(nsISupports* outer);

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init();

  NS_IMETHOD_(nsICalTimeContext *) GetTimeContext();
  NS_IMETHOD                       SetTimeContext(nsICalTimeContext * aContext);
  NS_IMETHOD_(PRUint32) GetVisibleMajorIntervals();
  NS_IMETHOD_(PRUint32) GetVisibleMinorIntervals();

  NS_IMETHOD_(nsEventStatus) PaintBackground(nsIRenderingContext& aRenderingContext,
                                             const nsRect& aDirtyRect);
  NS_IMETHOD SetParameter(nsString& aKey, nsString& aValue) ;

  NS_IMETHOD SetChildTimeContext(nsCalTimebarCanvas * aCanvas,
                                 nsICalTimeContext * aContext,
                                 PRUint32 increment);
  NS_IMETHOD ChangeChildDateTime(nsCalTimebarCanvas * aCanvas,
                                 nsDateTime * aDateTime);
  NS_IMETHOD ChangeChildDateTime(PRUint32 aIndex, nsDateTime * aDateTime);


protected:

  NS_IMETHOD PaintInterval(nsIRenderingContext& aRenderingContext,
                           const nsRect& aDirtyRect,
                           PRUint32 aIndex,
                           PRUint32 aStart,
                           PRUint32 aSpace,
                           PRUint32 aMinorInterval);

protected:
  ~nsCalTimebarCanvas();

// XXX: This needs to be multiply aggregated, I think.
private:
  nsICalTimeContext * mTimeContext;


};

#endif /* nsCalTimebarCanvas_h___ */
