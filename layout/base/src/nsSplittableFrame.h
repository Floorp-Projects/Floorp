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
#ifndef nsSplittableFrame_h___
#define nsSplittableFrame_h___

#include "nsFrame.h"

// Derived class that allows splitting
class nsSplittableFrame : public nsFrame
{
public:
  // Flow member functions.

  // CreateContinuingFrame() does the default behavior of using the
  // content delegate to create a new frame
  virtual PRBool      IsSplittable() const;
  virtual nsIFrame*   CreateContinuingFrame(nsIPresContext* aPresContext,
                                            nsIFrame*       aParent);
  virtual nsIFrame*   GetPrevInFlow() const;
  virtual void        SetPrevInFlow(nsIFrame*);
  virtual nsIFrame*   GetNextInFlow() const;
  virtual void        SetNextInFlow(nsIFrame*);

  /**
   * Return the first frame in our current flow. 
   */
  nsIFrame* GetFirstInFlow() const;

  /**
   * Return the last frame in our current flow.
   */
  nsIFrame* GetLastInFlow() const;

  virtual void        AppendToFlow(nsIFrame* aAfterFrame);
  virtual void        PrependToFlow(nsIFrame* aBeforeFrame);
  virtual void        RemoveFromFlow();
  virtual void        BreakFromPrevFlow();
  virtual void        BreakFromNextFlow();

protected:
  // Constructor. Takes as arguments the content object, the index in parent,
  // and the Frame for the content parent
  nsSplittableFrame(nsIContent* aContent,
                    PRInt32     aIndexInParent,
                    nsIFrame*   aParent);

  virtual ~nsSplittableFrame();

  nsIFrame*   mPrevInFlow;
  nsIFrame*   mNextInFlow;
};

#endif /* nsSplittableFrame_h___ */
