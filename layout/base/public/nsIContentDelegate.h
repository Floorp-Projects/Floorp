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
#ifndef nsIContentDelegate_h___
#define nsIContentDelegate_h___

#include "nslayout.h"
class nsIFrame;
class nsIPresContext;

#define NS_ICONTENTDELEGATE_IID \
 { 0x0f135ce0, 0xa286, 0x11d1,	\
   {0x89, 0x73, 0x00, 0x60, 0x08, 0x91, 0x1b, 0x81} }

/**
 * API for content delegation. Content objects create these objects to
 * provide hooks to creating their visual presentation.
 */
class nsIContentDelegate : public nsISupports {
public:
  /**
   * Create a new frame object that will be responsible for the layout
   * and presentation of the content.
   */
  virtual nsIFrame* CreateFrame(nsIPresContext* aPresContext,
                                nsIContent* aContent,
                                nsIFrame* aParentFrame) = 0;
};

#endif /* nsIContentDelegate_h___ */
