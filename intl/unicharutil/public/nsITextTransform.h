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
#ifndef nsITextTransform_h__
#define nsITextTransform_h__


#include "nsISupports.h"
#include "nscore.h"

// {CCD4D371-CCDC-11d2-B3B1-00805F8A6670}
#define NS_ITEXTTRANSFORM_IID \
{ 0xccd4d371, 0xccdc, 0x11d2, \
    { 0xb3, 0xb1, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } };


class nsITextTransform : public nsISupports {

public: 

  // Convert one Unicode character into upper case
  NS_IMETHOD Change( nsString& aText, nsString& aResult) = 0;

};

#endif  /* nsITextTransform_h__ */
