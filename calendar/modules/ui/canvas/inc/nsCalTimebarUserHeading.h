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

#ifndef nsCalTimebarUserHeading_h___
#define nsCalTimebarUserHeading_h___

#include "nsCalTimebarHeading.h"
#include "nsString.h"

class nsCalTimebarUserHeading : public nsCalTimebarHeading
                                    
{
public:
  nsCalTimebarUserHeading(nsISupports* outer);

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init();
  NS_IMETHOD_(nsEventStatus) PaintForeground(nsIRenderingContext& aRenderingContext,
                                             const nsRect& aDirtyRect);
  NS_IMETHOD SetParameter(nsString& aKey, nsString& aValue) ;

  NS_IMETHOD_(nsString&) GetUserName();
  NS_IMETHOD             SetUserName(nsString& aString);
  NS_IMETHOD GetClassPreferredSize(nsSize& aSize);


protected:
  ~nsCalTimebarUserHeading();

private:
  nsString mUserName;

};

#endif /* nsCalTimebarUserHeading_h___ */
