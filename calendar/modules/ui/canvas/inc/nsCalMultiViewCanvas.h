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

#ifndef nsCalMultiViewCanvas_h___
#define nsCalMultiViewCanvas_h___

#include "nsCalTimebarComponentCanvas.h"
#include "nsCalTimebarCanvas.h"
#include "nsDateTime.h"

class nsCalMultiViewCanvas : public nsCalTimebarComponentCanvas
{

public:
  nsCalMultiViewCanvas(nsISupports* outer);

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init();

  NS_IMETHOD SetShowHeaders(PRBool aShowHeaders);
  NS_IMETHOD_(PRBool) GetShowHeaders();

  NS_IMETHOD SetShowStatus(PRBool aShowHeaders);
  NS_IMETHOD_(PRBool) GetShowStatus();

  NS_IMETHOD SetShowTimeScale(PRBool aShowTimeScale);
  NS_IMETHOD_(PRBool) GetShowTimeScale();

  NS_IMETHOD SetParameter(nsString& aKey, nsString& aValue) ;
  NS_IMETHOD_(nsEventStatus) Action(nsIXPFCCommand * aCommand);

  NS_IMETHOD_(nsEventStatus) PaintBackground(nsIRenderingContext& aRenderingContext,
                                             const nsRect& aDirtyRect);

protected:
  ~nsCalMultiViewCanvas();

private:
  PRBool mShowHeaders;
  PRBool mShowStatus;
  PRBool mShowTimeScale;

};

#endif /* nsCalMultiViewCanvas_h___ */
