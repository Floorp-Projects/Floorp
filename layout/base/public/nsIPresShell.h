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
#ifndef nsIPresShell_h___
#define nsIPresShell_h___

#include "nslayout.h"
#include "nsISupports.h"
#include "nsCoord.h"
class nsIContent;
class nsIDocument;
class nsIDocumentObserver;
class nsIFrame;
class nsIPresContext;
class nsIStyleSet;
class nsIViewManager;
class nsIReflowCommand;
class nsIDeviceContext;
class nsIRenderingContext;
class nsIPageSequenceFrame;

#define NS_IPRESSHELL_IID     \
{ 0x76e79c60, 0x944e, 0x11d1, \
  {0x93, 0x23, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

/**
 * Presentation shell interface. Presentation shells are the
 * controlling point for managing the presentation of a document. The
 * presentation shell holds a live reference to the document, the
 * presentation context, the style manager, the style set and the root
 * frame. <p>
 *
 * When this object is Release'd, it will release the document, the
 * presentation context, the style manager, the style set and the root
 * frame.
 */
class nsIPresShell : public nsISupports {
public:
  NS_IMETHOD Init(nsIDocument* aDocument,
                  nsIPresContext* aPresContext,
                  nsIViewManager* aViewManager,
                  nsIStyleSet* aStyleSet) = 0;

  virtual nsIDocument* GetDocument() = 0;

  virtual nsIPresContext* GetPresContext() = 0;

  virtual nsIViewManager * GetViewManager() = 0;

  virtual nsIStyleSet* GetStyleSet() = 0;

  NS_IMETHOD EnterReflowLock() = 0;

  NS_IMETHOD ExitReflowLock() = 0;

  // Make shell be a document observer
  virtual void BeginObservingDocument() = 0;

  // Make shell stop being a document observer
  virtual void EndObservingDocument() = 0;

  /**
   * Perform the initial reflow. Constructs the frame for the root content
   * object and then reflows the frame model into the specified width and
   * height.
   *
   * The coordinates for aWidth and aHeight must be in standard nscoord's.
   */
  NS_IMETHOD InitialReflow(nscoord aWidth, nscoord aHeight) = 0;

  /**
   * Reflow the frame model into a new width and height.  The
   * coordinates for aWidth and aHeight must be in standard nscoord's.
   */
  NS_IMETHOD ResizeReflow(nscoord aWidth, nscoord aHeight) = 0;

  /**
   * Reflow the frame model with a reflow reason of eReflowReason_StyleChange
   */
  NS_IMETHOD StyleChangeReflow() = 0;

  virtual nsIFrame* GetRootFrame() = 0;

  /**
   * Returns the page sequence frame associated with the frame hierarchy.
   * Returns NULL if not a paginated view.
   */
  NS_IMETHOD GetPageSequenceFrame(nsIPageSequenceFrame*& aPageSequenceFrame) = 0;

  virtual nsIFrame* FindFrameWithContent(nsIContent* aContent) = 0;

  virtual void AppendReflowCommand(nsIReflowCommand* aReflowCommand) = 0;

  virtual void ProcessReflowCommands() = 0;

  virtual void ClearFrameRefs(nsIFrame* aFrame) = 0;
  /**
   * Given a frame, cough up a rendering context suitable for use with
   * the frame.
   */
  NS_IMETHOD CreateRenderingContext(nsIFrame *aFrame, nsIRenderingContext *&aContext) = 0;

  // XXX events
  // XXX selection

  /**
   * See if reflow verification is enabled. To enable reflow verification add
   * "verifyreflow:1" to your NSPR_LOG_MODULES environment variable
   * (any non-zero debug level will work). Or, call SetVerifyReflowEnable
   * with PR_TRUE.
   */
  static NS_LAYOUT PRBool GetVerifyReflowEnable();

  /**
   * Set the verify-reflow enable flag.
   */
  static NS_LAYOUT void SetVerifyReflowEnable(PRBool aEnabled);
};

/**
 * Create a new empty presentation shell. Upon success, call Init
 * before attempting to use the shell.
 */
extern NS_LAYOUT nsresult
  NS_NewPresShell(nsIPresShell** aInstancePtrResult);

#endif /* nsIPresShell_h___ */
