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

#ifndef nsCalMultiUserViewCanvas_h___
#define nsCalMultiUserViewCanvas_h___

#include "nsCalMultiViewCanvas.h"
#include "nsCalTimebarCanvas.h"
#include "nsDateTime.h"
#include "nsIModel.h"
#include "nsBoxLayout.h"

class nsCalMultiUserViewCanvas : public nsCalMultiViewCanvas
{

public:
  nsCalMultiUserViewCanvas(nsISupports* outer);

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init();
  NS_IMETHOD SetParameter(nsString& aKey, nsString& aValue) ;
  NS_IMETHOD SetTimeContext(nsICalTimeContext * aContext);

  // nsIXPFCCommandReceiver methods
  NS_IMETHOD_(nsEventStatus) Action(nsIXPFCCommand * aCommand);

protected:
  ~nsCalMultiUserViewCanvas();

private:
  NS_IMETHOD AddMultiDayView(nsIModel * aModel);
  NS_IMETHOD SetMultiUserLayout(nsLayoutAlignment aLayoutAlignment);


};

#endif /* nsCalMultiUserViewCanvas_h___ */
