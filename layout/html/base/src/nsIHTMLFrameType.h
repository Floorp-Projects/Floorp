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
#ifndef nsIHTMLFrameType_h___
#define nsIHTMLFrameType_h___

#include "nsISupports.h"

enum nsHTMLFrameType {
  eHTMLFrame_Block,
};

// Interface for nsIHTMLFrameType
#define NS_IHTMLFRAMETYPE_IID                \
{ /* 0e2554e0-b2c8-11d1-9328-00805f8add32 */ \
  0x0e2554e0, 0xb2c8, 0x11d1,                \
    {0x93, 0x28, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

// IHTMLFrameType interface. Note that this interface is not an
// nsISupports interface and therefore you cannot queryinterface back
// to the original object.
class nsIHTMLFrameType {
public:
  virtual nsHTMLFrameType GetFrameType() const = 0;
};

#endif /* nsIHTMLFrameType_h___ */
