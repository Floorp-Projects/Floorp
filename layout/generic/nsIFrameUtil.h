/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#ifndef nsIFrameUtil_h___
#define nsIFrameUtil_h___

#include "nsIXMLContent.h"
class nsIURL;

/* a6cf90d4-15b3-11d2-932e-00805f8add32 */
#define NS_IFRAME_UTIL_IID \
 { 0xa6cf90d6, 0x15b3, 0x11d2,{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

/**
 * Frame utility interface
 */
class nsIFrameUtil : public nsISupports {
public:
  /**
   * Load frame regression data from the given URL synchronously.
   * Once the data is loaded and translated into a content tree,
   * return in aResult a pointer to the root of the tree.
   */
  NS_IMETHOD LoadFrameRegressionData(nsIURL* aURL,
                                     nsIXMLContent** aResult) = 0;

  // XXX temporary until sync i/o works again
  NS_IMETHOD ReadFrameRegressionData(FILE* aInput,
                                     nsIXMLContent** aResult) = 0;
};

#endif /* nsIFrameUtil_h___ */
