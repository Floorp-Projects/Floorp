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
#ifndef nsITextContent_h___
#define nsITextContent_h___

#include "nslayout.h"
class nsString;

// IID for the nsITextContent interface
#define NS_ITEXTCONTENT_IID   \
{ 0xdd0755d0, 0x944d, 0x11d1, \
  {0x93, 0x23, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

// Abstract interface for textual content. Note that this interface
// does not implement nsISupports (which means it's not transitive or
// reflexive). This is done for efficiency reasons.
class nsITextContent {
public:
  /*
   * Get the total length of the text content.
   */
  virtual PRInt32 GetLength() = 0;

  /*
   * Copy a subrange of the text content into aBuf starting at aOffset
   * for aCount characters. aBuf's length will be reset before the
   * copy occurs and it's length upon return will reflect the amount
   * of data copied.
   */
  virtual void GetText(nsString& aBuf, PRInt32 aOffset, PRInt32 aCount) = 0;
};

#endif /* nsITextContent_h___ */
