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

#include "nsFrame.h"

// Implementation of a frame that's used as a placeholder for an anchored item
class PlaceholderFrame : public nsFrame
{
public:
  /**
   * Create a new placeholder frame
   */
  static nsresult NewFrame(nsIFrame**  aInstancePtrResult,
                           nsIContent* aContent,
                           PRInt32     aIndexInParent,
                           nsIFrame*   aParent);

  // Returns the associated anchored item
  nsIFrame*   GetAnchoredItem() const {return mAnchoredItem;}

  // Resize reflow methods
  virtual ReflowStatus  ResizeReflow(nsIPresContext*  aPresContext,
                                     nsReflowMetrics& aDesiredSize,
                                     const nsSize&    aMaxSize,
                                     nsSize*          aMaxElementSize);
  virtual void          ListTag(FILE* out = stdout) const;

protected:
  // Constructor. Takes as arguments the content object, the index in parent,
  // and the Frame for the content parent
  PlaceholderFrame(nsIContent* aContent,
                   PRInt32     aIndexInParent,
                   nsIFrame*   aParent);

  virtual ~PlaceholderFrame();

  nsIFrame*   mAnchoredItem;
};

#endif /* nsPlaceholderFrame_h___ */
