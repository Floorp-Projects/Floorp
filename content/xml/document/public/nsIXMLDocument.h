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

#ifndef nsIXMLDocument_h___
#define nsIXMLDocument_h___

#include "nsISupports.h"
class nsIURI;

#define NS_IXMLDOCUMENT_IID \
 { 0xa6cf90ca, 0x15b3, 0x11d2, \
 { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } }

/**
 * XML document extensions to nsIDocument
 */
class nsIXMLDocument : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IXMLDOCUMENT_IID)

  NS_IMETHOD SetDefaultStylesheets(nsIURI* aUrl)=0;
};

#endif // nsIXMLDocument_h___
