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
#ifndef nsIFrameManager_h___
#define nsIFrameManager_h___

#include "nslayout.h"
#include "nsISupports.h"

class nsIAtom;
class nsIContent;
class nsIFrame;
class nsIPresContext;
class nsIPresShell;
class nsIStyleSet;
class nsIStyleContext;
class nsILayoutHistoryState;

#define NS_IFRAMEMANAGER_IID     \
{ 0xa6cf9107, 0x15b3, 0x11d2, \
  {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

/**
 * Frame manager interface. The frame manager serves two purposes:
 * <li>provides a serice for mapping from content to frame and from out-of-flow
 * frame to placeholder frame
 * <li>handles structural modifications to the frame model. If the frame model
 * lock can be acquired, then the changes are processed immediately; otherwise,
 * they're queued and processed later
 */
class nsIFrameManager : public nsISupports {
public:
  static const nsIID& GetIID() {static nsIID iid = NS_IFRAMEMANAGER_IID; return iid;}

  // Initialization
  NS_IMETHOD Init(nsIPresShell* aPresShell, nsIStyleSet* aStyleSet) = 0;

  // Primary frame functions
  NS_IMETHOD GetPrimaryFrameFor(nsIContent* aContent, nsIFrame** aPrimaryFrame) = 0;
  NS_IMETHOD SetPrimaryFrameFor(nsIContent* aContent,
                                nsIFrame*   aPrimaryFrame) = 0;
  NS_IMETHOD ClearPrimaryFrameMap() = 0;

  // Placeholder frame functions
  NS_IMETHOD GetPlaceholderFrameFor(nsIFrame*  aFrame,
                                    nsIFrame** aPlaceholderFrame) const = 0;
  NS_IMETHOD SetPlaceholderFrameFor(nsIFrame* aFrame,
                                    nsIFrame* aPlaceholderFrame) = 0;
  NS_IMETHOD ClearPlaceholderFrameMap() = 0;

  // Functions for manipulating the frame model
  NS_IMETHOD AppendFrames(nsIPresContext& aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIFrame*       aParentFrame,
                          nsIAtom*        aListName,
                          nsIFrame*       aFrameList) = 0;
  NS_IMETHOD InsertFrames(nsIPresContext& aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIFrame*       aParentFrame,
                          nsIAtom*        aListName,
                          nsIFrame*       aPrevFrame,
                          nsIFrame*       aFrameList) = 0;
  NS_IMETHOD RemoveFrame(nsIPresContext& aPresContext,
                         nsIPresShell&   aPresShell,
                         nsIFrame*       aParentFrame,
                         nsIAtom*        aListName,
                         nsIFrame*       aOldFrame) = 0;
  NS_IMETHOD ReplaceFrame(nsIPresContext& aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIFrame*       aParentFrame,
                          nsIAtom*        aListName,
                          nsIFrame*       aOldFrame,
                          nsIFrame*       aNewFrame) = 0;

  // Notification that we were unable to render a replaced element
  NS_IMETHOD CantRenderReplacedElement(nsIPresContext* aPresContext,
                                       nsIFrame*       aFrame) = 0;

  // Notification that a frame is about to be destroyed. This allows any outstanding
  // references to the frame to be cleaned up
  NS_IMETHOD NotifyDestroyingFrame(nsIFrame* aFrame) = 0;

  // reparent the style contexts of this frame sub tree to live under the
  // new given parent style context
  NS_IMETHOD ReParentStyleContext(nsIPresContext& aPresContext, 
                                  nsIFrame* aFrame, 
                                  nsIStyleContext* aNewParentContext) = 0;

  /**
   * Capture/restore frame state for the frame subtree rooted at aFrame.
   * aState is the document state storage object onto which each frame 
   * stores its state.
   */
  NS_IMETHOD CaptureFrameState(nsIFrame* aFrame, nsILayoutHistoryState* aState) = 0;
  NS_IMETHOD RestoreFrameState(nsIFrame* aFrame, nsILayoutHistoryState* aState) = 0;
};

/**
 * Create a frame manager. Upon success, call Init() before attempting to
 * use it.
 */
extern NS_LAYOUT nsresult
  NS_NewFrameManager(nsIFrameManager** aInstancePtrResult);

#endif /* nsIFrameManager_h___ */
