/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsIXMLEncodingService_h__
#define nsIXMLEncodingService_h__
#include "nsISupports.h"


// {12BB8F11-2389-11d3-B3BF-00805F8A6670}
#define NS_IXML_ENCODING_SERVICE_IID \
{ 0x12bb8f11, 0x2389, 0x11d3, { 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }


class nsIXMLEncodingService : public nsISupports {
public:
   NS_DEFINE_STATIC_IID_ACCESSOR( NS_IXML_ENCODING_SERVICE_IID )

   NS_IMETHOD Start() = 0;
   NS_IMETHOD End() = 0;
};
#endif // nsIXMLEncodingService_h__
