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
#ifndef nsIStyleFrameConstruction_h___
#define nsIStyleFrameConstruction_h___

#include "nsISupports.h"

class nsIPresContext;
class nsIContent;
class nsIFrame;
class nsIAtom;
class nsIStyleSheet;
class nsIStyleRule;
class nsStyleChangeList;

// IID for the nsIStyleSet interface {a6cf9066-15b3-11d2-932e-00805f8add32}
#define NS_ISTYLE_FRAME_CONSTRUCTION_IID \
{0xa6cf9066, 0x15b3, 0x11d2, {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

class nsIStyleFrameConstruction : public nsISupports {
public:
  /**
   * Create frames for the root content element and its child content.
   */
  NS_IMETHOD ConstructRootFrame(nsIPresContext* aPresContext,
                                nsIContent*     aDocElement,
                                nsIFrame*&      aFrameSubTree) = 0;

  // Causes reconstruction of a frame hierarchy rooted by the
  // document element frame. This is often called when radical style
  // change precludes incremental reflow.
  NS_IMETHOD  ReconstructDocElementHierarchy(nsIPresContext* aPresContext) = 0;

  NS_IMETHOD ContentAppended(nsIPresContext* aPresContext,
                             nsIContent*     aContainer,
                             PRInt32         aNewIndexInContainer) = 0;

  NS_IMETHOD ContentInserted(nsIPresContext* aPresContext,
                             nsIContent*     aContainer,
                             nsIContent*     aChild,
                             PRInt32         aIndexInContainer) = 0;

  NS_IMETHOD ContentReplaced(nsIPresContext* aPresContext,
                             nsIContent*     aContainer,
                             nsIContent*     aOldChild,
                             nsIContent*     aNewChild,
                             PRInt32         aIndexInContainer) = 0;

  NS_IMETHOD ContentRemoved(nsIPresContext* aPresContext,
                            nsIContent*     aContainer,
                            nsIContent*     aChild,
                            PRInt32         aIndexInContainer) = 0;

  NS_IMETHOD ContentChanged(nsIPresContext* aPresContext,
                            nsIContent* aContent,
                            nsISupports* aSubContent) = 0;

  NS_IMETHOD ContentStateChanged(nsIPresContext* aPresContext, 
                                 nsIContent* aContent) = 0;

  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent* aContent,
                              nsIAtom* aAttribute,
                              PRInt32 aHint) = 0;

  // Style change notifications
  NS_IMETHOD StyleRuleChanged(nsIPresContext* aPresContext,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule,
                              PRInt32 aHint) = 0; // See nsStyleConsts fot hint values
  NS_IMETHOD StyleRuleAdded(nsIPresContext* aPresContext,
                            nsIStyleSheet* aStyleSheet,
                            nsIStyleRule* aStyleRule) = 0;
  NS_IMETHOD StyleRuleRemoved(nsIPresContext* aPresContext,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule) = 0;
  NS_IMETHOD ProcessRestyledFrames(nsStyleChangeList& aRestyleArray, 
                                   nsIPresContext* aPresContext) = 0;
  
  // Notification that we were unable to render a replaced element.
  NS_IMETHOD CantRenderReplacedElement(nsIPresContext* aPresContext,
                                       nsIFrame*       aFrame) = 0;

  // Request to create a continuing frame
  NS_IMETHOD CreateContinuingFrame(nsIPresContext* aPresContext,
                                   nsIFrame*       aFrame,
                                   nsIFrame*       aParentFrame,
                                   nsIFrame**      aContinuingFrame) = 0;
};

#endif /* nsIStyleFrameConstruction_h___ */
