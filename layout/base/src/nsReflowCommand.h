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
#ifndef nsReflowCommand_h___
#define nsReflowCommand_h___

#include "nsIFrame.h"
#include "nsVoidArray.h"

class nsIPresContext;
class nsISpaceManager;

// A reflow command.
class nsReflowCommand {
public:
  enum ReflowType {
    // Reflow commands generated in response to a content insert/delete/append
    // notification. The target of the reflow command is the container frame
    // itself
    FrameAppended,
    FrameInserted,
    FrameDeleted,

    // This reflow command is used when a leaf node's content changes (e.g. some
    // text in a text run, an image's source, etc.). The target of the reflow
    // command is the frame that changed (see nsIFrame#ContentChanged() for how
    // the target frame is determined).
    ContentChanged,

    // When an incremental reflow operation affects a next-in-flow,
    // these commands are used to get the next-in-flow to update
    // itself.
    PullupReflow,
    PushReflow,

    // This command is used to see if a prev-in-flow wants to pullup
    // some children from a next-in-flow that has changed because of
    // an incremental reflow.
    CheckPullupReflow,

    // Trap door for extensions
    UserDefined
  };

  // XXX factory methods?

  nsReflowCommand(nsIPresContext* aPresContext,
                  nsIFrame*       aTargetFrame,
                  ReflowType      aReflowType);

  nsReflowCommand(nsIPresContext* aPresContext,
                  nsIFrame*       aTargetFrame,
                  ReflowType      aReflowType,
                  nsIFrame*       aChildFrame);

  virtual ~nsReflowCommand();

  /**
   * Dispatch the reflow command.
   *
   * Builds a path from the target frame back to the root frame, and then invokes
   * the root frame's Reflow() member function.
   */
  void Dispatch(nsReflowMetrics& aDesiredSize, const nsSize& aMaxSize);

  /**
   * Get the next frame in the command chain. Note that this removes the frame
   * from the target chain
   */
  nsIFrame* GetNext();

  /**
   * Get the target of the reflow command
   */
  nsIFrame* GetTarget() const {return mTargetFrame;}

  /**
   * Get the type of reflow command
   */
  ReflowType GetType() const {return mType;}

  /**
   * Get the child frame associated with the reflow command
   */
  nsIFrame*  GetChildFrame() const {return mChildFrame;}

private:
  nsIPresContext* mPresContext;
  ReflowType      mType;
  nsIFrame*       mTargetFrame;
  nsIFrame*       mChildFrame;
  nsVoidArray     mPath;
};

#endif /* nsReflowCommand_h___ */
