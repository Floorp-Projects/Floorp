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
#ifndef nsHankakuToZenkakuCID_h__
#define nsHankakuToZenkakuCID_h__


#include "nsITextTransform.h"
#include "nsISupports.h"
#include "nscore.h"

// {8F666A11-04A0-11d3-B3B9-00805F8A6670}
#define NS_HANKAKUTOZENKAKU_CID \
{ 0x8f666a11, 0x4a0, 0x11d3, \
  { 0xb3, 0xb9, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }

#define NS_HANKAKUTOZENKAKU_PROGID NS_TEXTTRANSFORM_PROGID_BASE "hankakutozenkaku"


#endif
