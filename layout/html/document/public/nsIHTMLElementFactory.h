/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#ifndef nsIHTMLElementFactory_h___
#define nsIHTMLElementFactory_h___

#include "nslayout.h"
#include "nsISupports.h"

class nsIHTMLContent;
class nsString;

/* a6cf90fb-15b3-11d2-932e-00805f8add32 */
#define NS_IHTML_ELEMENT_FACTORY_IID \
 { 0xa6cf90fb, 0x15b3, 0x11d2,{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

/**
 * An API for creating html content objects
 */
class nsIHTMLElementFactory : public nsISupports {
public:
  NS_IMETHOD CreateInstanceByTag(const nsString& aTag,
                                 nsIHTMLContent** aResult) = 0;
};

#endif /* nsIHTMLElementFactory_h___ */
