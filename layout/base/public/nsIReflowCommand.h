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
#ifndef nsIReflowCommand_h___
#define nsIReflowCommand_h___

#include "nsISupports.h"

class  nsIFrame;
class  nsIPresContext;
struct nsReflowMetrics;
struct nsSize;

// IID for the nsIReflowCommand interface {C3658E40-FF20-11d1-85BC-00A02468FAB6}
#define NS_IREFLOWCOMMAND_IID         \
{ 0xc3658e40, 0xff20, 0x11d1, \
  {0x85, 0xbc, 0x0, 0xa0, 0x24, 0x68, 0xfa, 0xb6}}

/**
 * A reflow command is an object that is generated in response to a content
 * model change notification. The reflow command is given to a presentation
 * shell where it is queued and then dispatched by invoking the reflow
 * commands's Dispatch() member function.
 *
 * Reflow command processing follows a path from the root frame down to the
 * target frame (the frame for which the reflow command is destined). Reflow
 * commands are processed by invoking the frame's Reflow() member function.
 *
 * @see nsIFrame#ContentAppended()
 * @see nsIFrame#ContentChanged()
 * @see nsIFrame#ContentDeleted()
 * @see nsIFrame#ContentInserted()
 * @see nsIFrame#ContentReplaced()
 * @see nsIPresShell#AppendReflowCommand()
 * @see nsIPresShell#ProcessReflowCommands()
 */
class nsIReflowCommand : public nsISupports {
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

  /**
   * Dispatch the reflow command.
   *
   * Builds a path from the target frame back to the root frame, and then
   * invokes the root frame's Reflow() member function.
   *
   * @see nsIFrame#Reflow()
   */
  NS_IMETHOD Dispatch(nsIPresContext&  aPresContext,
                      nsReflowMetrics& aDesiredSize,
                      const nsSize&    aMaxSize) = 0;

  /**
   * Get the next frame in the command processing path. Note that this removes
   * the frame from the path so you must only call it once.
   */
  NS_IMETHOD GetNext(nsIFrame*& aNextFrame) = 0;

  /**
   * Get the target of the reflow command.
   */
  NS_IMETHOD GetTarget(nsIFrame*& aTargetFrame) const = 0;

  /**
   * Get the type of reflow command.
   */
  NS_IMETHOD GetType(ReflowType& aReflowType) const = 0;

  /**
   * Get the child frame associated with the reflow command.
   */
  NS_IMETHOD GetChildFrame(nsIFrame*& aChildFrame) const = 0;
};

#endif /* nsIReflowCommand_h___ */
