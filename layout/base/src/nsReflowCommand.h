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
    // These reflow types are used for pre-processing a content
    // append, insert or delete. The target frame is responsible for
    // creating frames for the new children (or removing frames for
    // delete) and then updating it's children's index-in-parent
    // values. After that, the frame is responsible for creating new
    // reflow commands to deal with the appended, inserted or deleted
    // frames.
    ContentAppended,
    ContentInserted,
    ContentReplaced,
    ContentDeleted,

    // These are the reflow commands generated after the previous
    // commands have completed.
    FrameAppended,
    FrameInserted,
    FrameDeleted,

    // This reflow command is used when a leaf node's content changes
    // (e.g. some text in a text run, an image's source, etc.)
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
                  nsIFrame* aTargetFrame,
                  ReflowType aReflowType);

  nsReflowCommand(nsIPresContext* aPresContext,
                  nsIFrame* aTargetFrame,
                  ReflowType aReflowType,
                  PRInt32 aIndexValue);

  nsReflowCommand(nsIPresContext* aPresContext,
                  nsIFrame* aTargetFrame,
                  ReflowType aReflowType,
                  nsIContent* aContainer);

  nsReflowCommand(nsIPresContext* aPresContext,
                  nsIFrame* aTargetFrame,
                  ReflowType aReflowType,
                  nsIContent* aContainer,
                  nsIContent* aChild,
                  PRInt32 aIndexInParent);

  nsReflowCommand(nsIPresContext* aPresContext,
                  nsIFrame* aTargetFrame,
                  ReflowType aReflowType,
                  nsIContent* aContainer,
                  nsIContent* aOldChild,
                  nsIContent* aNewChild,
                  PRInt32 aIndexInParent);

  virtual ~nsReflowCommand();

  /**
   * Dispatch the reflow command. The command is processed and dispatched
   * to the appropriate frame's appropriate method.
   */
  void Dispatch(nsReflowMetrics& aDesiredSize, const nsSize& aMaxSize);

  // Pass the reflow command to the next frame in the hierarchy. Returns the
  // status and the next frame to which the command was dispatched
  nsIFrame::ReflowStatus Next(nsReflowMetrics& aDesiredSize,
                              const nsSize&    aMaxSize,
                              nsIFrame*&       aNextFrame);

  // Pass the reflow command to the next frame in the hierarchy. Returns the
  // status and the next frame to which the command was dispatched
  //
  // Use this version if you have a space manager. This function will check if
  // the caller supports nsIRunaround and call either the nsIFrame or the
  // nsIRunaround IncrementalReflow() function
  nsIFrame::ReflowStatus Next(nsISpaceManager* aSpaceManager,
                              nsRect&          aDesiredRect,
                              const nsSize&    aMaxSize,
                              nsIFrame*&       aNextFrame);

  nsIFrame* GetNext() const;

  // Get the target of the reflow command
  nsIFrame* GetTarget() const { return mTargetFrame; }

  // Get the type of reflow command
  ReflowType GetType() const { return mType; }

  // Get the index value
  PRInt32 GetIndex() const { return mIndex; }

private:
  nsIPresContext* mPresContext;
  ReflowType      mType;
  nsIContent*     mContainer;
  nsIContent*     mChild;
  nsIContent*     mOldChild;
  PRInt32         mIndex;
  nsIFrame*       mTargetFrame;
  nsVoidArray     mPath;
};

#endif /* nsReflowCommand_h___ */
