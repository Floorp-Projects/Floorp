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
#ifndef nsIFocusableContent_h___
#define nsIFocusableContent_h___

#include "nsISupports.h"
class nsIPresContext;

#define NS_IFOCUSABLECONTENT_IID   \
{ 0xc5df3c30, 0xc79a, 0x11d2, \
  {0xbd, 0x94, 0x00, 0x00, 0x80, 0x5f, 0x8a, 0x81} }

/**
  * Interface which all focusable content (e.g. form elements, links
  * , etc) implements in addition to their dom specific interface. 
 **/
class nsIFocusableContent : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IFOCUSABLECONTENT_IID; return iid; }

  /**
    * Give focus to the content
    * @param aPresContext the PesContext
    * @return NS_OK
    */
  NS_IMETHOD SetFocus(nsIPresContext* aPresContext) = 0;

  /**
    * Remove focus to the content
    * @param aPresContext the PesContext
    * @return NS_OK
    */
  NS_IMETHOD RemoveFocus(nsIPresContext* aPresContext) = 0;

};

#endif /* nsIFocusableContent_h___ */
