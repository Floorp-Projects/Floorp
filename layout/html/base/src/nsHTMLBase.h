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
#ifndef nsHTMLBase_h___
#define nsHTMLBase_h___

#include "nsIFrame.h"

// This is a class whose purpose is to provide shared code that is
// common to all html frames, including containers and leaf frames.
class nsHTMLBase {
public:
  /**
   * Create a view for a frame if it needs one. A frame needs a view
   * if it requires a scrollbar, is relatively positioned or has a
   * non opaque opacity.
   */
  static nsresult CreateViewForFrame(nsIPresContext*  aPresContext,
                                     nsIFrame*        aFrame,
                                     nsIStyleContext* aStyleContext,
                                     PRBool           aForce);

  /**
   * Create a frame for a given piece of content using the style
   * as a guide for determining which frame to create.
   */
  static nsresult CreateFrame(nsIPresContext* aPresContext,
                              nsIFrame*       aParentFrame,
                              nsIContent*     aKid,
                              nsIFrame*       aKidPrevInFlow,
                              nsIFrame*&      aResult);
};

#endif /* nsHTMLBase_h___ */
