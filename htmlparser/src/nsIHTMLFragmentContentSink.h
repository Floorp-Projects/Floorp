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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsIHTMLFragmentContentSink_h___
#define nsIHTMLFragmentContentSink_h___

#include "nsIHTMLContentSink.h"

class nsIDOMDocumentFragment;

#define NS_IHTML_FRAGMENT_CONTENT_SINK_IID \
 {0xa6cf9102, 0x15b3, 0x11d2,              \
 {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

class nsIHTMLFragmentContentSink : public nsIHTMLContentSink {
public:
  /**
   * This method is used to obtain the fragment created by
   * a fragment content sink. The value returned will be null
   * if the content sink hasn't yet received parser notifications.
   *
   */
  NS_IMETHOD GetFragment(nsIDOMDocumentFragment** aFragment) = 0;
};

#endif
