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
#ifndef nsIStyleSheetLinkingElement_h__
#define nsIStyleSheetLinkingElement_h__


#include "nsISupports.h"

#define NS_ISTYLESHEETLINKINGELEMENT_IID          \
  {0xa6cf90e9, 0x15b3, 0x11d2,                    \
  {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

class nsIStyleSheet;

class nsIStyleSheetLinkingElement : public nsISupports {
public:
  /**
   * Used to make the association between a style sheet and
   * the element that linked it to the document.
   *
   * @param aStyleSheet the style sheet associated with this
   *                    element.
   */
  NS_IMETHOD SetStyleSheet(nsIStyleSheet* aStyleSheet) = 0;

  /**
   * Used to obtain the style sheet linked in by this element.
   *
   * @param aStyleSheet out parameter that returns the style
   *                    sheet associated with this element.
   */
  NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aStyleSheet) = 0;
};

#endif // nsILinkingElement_h__
