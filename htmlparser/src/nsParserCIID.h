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

#ifndef nsParserCIID_h__
#define nsParserCIID_h__

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsRepository.h"

#define NS_PARSER_IID      \
  {0x2ce606b0, 0xbee6,  0x11d1,  \
  {0xaa, 0xd9, 0x00,    0x80, 0x5f, 0x8a, 0x3e, 0x14}}

// XXX: This object should not be exposed outside of the parser.
//      Remove when CNavDTD subclasses do not need access
#define NS_PARSER_NODE_IID      \
  {0x9039c670, 0x2717,  0x11d2,  \
  {0x92, 0x46, 0x00,    0x80, 0x5f, 0x8a, 0x7a, 0xb6}}

// {E6FD9941-899D-11d2-8EAE-00805F29F370}
#define NS_WELLFORMEDDTD_CID \
{ 0xe6fd9941, 0x899d, 0x11d2, { 0x8e, 0xae, 0x0, 0x80, 0x5f, 0x29, 0xf3, 0x70 } }


#endif
