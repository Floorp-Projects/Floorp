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
#ifndef nsCharsOrderIdFormater_h__
#define nsCharsOrderIdFormater_h__


#include "nsIOrderIdFormater.h"
#include "nsCharsList.h"

class nsCharsOrderIdFormater : public nsIOrderIdFormater {
  NS_DECL_ISUPPORTS

public: 
  
  nsCharsOrderIdFormater( nsCharsList* aList);

  virtual nsCharsOrderIdFormater();
  

  // Convert one Unicode character into upper case
  NS_IMETHOD ToString( PRUint32 aOrder, nsString& aResult) = 0;

private:

  nsCharsList *mList;
  PRUint32     mBase;
};

#endif  /* nsCharsOrderIdFormater_h__ */
