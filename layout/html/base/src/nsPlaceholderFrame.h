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
#ifndef nsPlaceholderFrame_h___
#define nsPlaceholderFrame_h___

#include "nsContainerFrame.h"

// Implementation of a frame that's used as a placeholder for an anchored item
class nsPlaceholderFrame : public nsContainerFrame {
public:
  /**
   * Create a new placeholder frame
   */
  static nsresult NewFrame(nsIFrame**  aInstancePtrResult,
                           nsIContent* aContent,
                           nsIFrame*   aParent);

  // Returns the associated anchored item
  nsIFrame*   GetAnchoredItem() const { return mFirstChild; }

  // nsIFrame overrides
  NS_IMETHOD  IsSplittable(nsSplittableType& aIsSplittable) const;
  NS_IMETHOD  Reflow(nsIPresContext*      aPresContext,
                     nsReflowMetrics&     aDesiredSize,
                     const nsReflowState& aReflowState,
                     nsReflowStatus&      aStatus);
  NS_IMETHOD  ListTag(FILE* out = stdout) const;

protected:
  // Constructor. Takes as arguments the content object, the index in parent,
  // and the Frame for the content parent
  nsPlaceholderFrame(nsIContent* aContent, nsIFrame* aParent);
  virtual ~nsPlaceholderFrame();
};

#endif /* nsPlaceholderFrame_h___ */
