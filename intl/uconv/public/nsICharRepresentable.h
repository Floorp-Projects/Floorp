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

#ifndef nsICharRepresentable_h__
#define nsICharRepresentable_h__

#include "nscore.h"
#include "nsISupports.h"

// {A4D9A521-185A-11d3-B3BD-00805F8A6670}
#define NS_ICHARREPRESENTABLE_IID \
{ 0xa4d9a521, 0x185a, 0x11d3, { 0xb3, 0xbd, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }


/* 
   The following two macro have been duplicate in umap.c. 
   You need to change both place to make it work 
*/
#define IS_REPRESENTABLE(info, c) (((info)[(c) >> 5] >> ((c) & 0x1f)) & 1L)
#define SET_REPRESENTABLE(info, c)  (info)[(c) >> 5] |= (1L << ((c) & 0x1f))
#define CLEAR_REPRESENTABLE(info, c)  (info)[(c) >> 5] &= (~(1L << ((c) & 0x1f)))

// number of PRUint32 in the 64Kbit char map
#define UCS2_MAP_LEN 2048

/**
 */
class nsICharRepresentable : public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ICHARREPRESENTABLE_IID; return iid; }

  NS_IMETHOD FillInfo(PRUint32* aInfo) = 0;

};

#endif /* nsIUnicodeDecoder_h__ */
