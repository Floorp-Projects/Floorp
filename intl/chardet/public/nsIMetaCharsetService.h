/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsIMetaCharsetService_h__
#define nsIMetaCharsetService_h__
#include "nsISupports.h"


// {218F2AC1-0A48-11d3-B3BA-00805F8A6670}
#define NS_IMETA_CHARSET_SERVICE_IID \
{ 0x218f2ac1, 0xa48, 0x11d3, { 0xb3, 0xba, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }

class nsIMetaCharsetService : public nsISupports {
public:
   NS_IMETHOD Start() = 0;
   NS_IMETHOD End() = 0;
};
#endif // nsIMetaCharsetService_h__
