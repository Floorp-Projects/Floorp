/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsIReflowCommand_h___
#define nsIReflowCommand_h___

#include "nsISupports.h"
#include <stdio.h>

class  nsIAtom;
class  nsIFrame;
class  nsIPresContext;
class  nsIRenderingContext;
struct nsHTMLReflowMetrics;
struct nsSize;

// IID for the nsIReflowCommand interface {C3658E40-FF20-11d1-85BC-00A02468FAB6}
#define NS_IREFLOWCOMMAND_IID         \
{ 0xc3658e40, 0xff20, 0x11d1, \
  {0x85, 0xbc, 0x0, 0xa0, 0x24, 0x68, 0xfa, 0xb6}}

// Reflow command flags
#define NS_RC_CREATED_DURING_DOCUMENT_LOAD 0x0001

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
 * The typical flow of control for a given reflow command starts with a content
 * change notification. The content notifications are sent to document observers.
 * The presentation shell forwards the notifications to the style set. The style
 * system responds to the notifications by creating new frame (or destroying
 * existing frames) as appropriate, and then generating a reflow command.
 *
 * @see nsIDocumentObserver
 * @see nsIStyleSet
 * @see nsIFrameReflow#Reflow()
 * @see nsIPresShell#AppendReflowCommand()
 * @see nsIPresShell#ProcessReflowCommands()
 */
class nsIReflowCommand : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IREFLOWCOMMAND_IID; return iid; }

  enum ReflowType {
    // This reflow command is used when a leaf node's content changes
    // (e.g. some text in a text run, an image's source, etc.). The
    // target of the reflow command is the frame that changed (see
    // nsIFrame#ContentChanged() for how the target frame is
    // determined).
    ContentChanged,

    // This reflow command is used when the style for a frame has
    // changed. This also implies that if the frame is a container
    // that its childrens style has also changed. The target of the
    // reflow command is the frame that changed style.
    StyleChanged,

    // Reflow dirty stuff (really a per-frame extension)
    ReflowDirty,

    // The pres shell ran out of time but will guaranteed the reflow command gets processed.
    Timeout,

    // Trap door for extensions.
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
  NS_IMETHOD Dispatch(nsIPresContext*      aPresContext,
                      nsHTMLReflowMetrics& aDesiredSize,
                      const nsSize&        aMaxSize,
                      nsIRenderingContext& aRendContext) = 0;

  /**
   * Get the next frame in the command processing path. If requested removes the
   * the frame from the path. You must remove the frame from the path before
   * dispatching the reflow command to the next frame in the chain.
   */
  NS_IMETHOD GetNext(nsIFrame*& aNextFrame, PRBool aRemove = PR_TRUE) = 0;

  /**
   * Get the target of the reflow command.
   */
  NS_IMETHOD GetTarget(nsIFrame*& aTargetFrame) const = 0;

  /**
   * Change the target of the reflow command.
   */
  NS_IMETHOD SetTarget(nsIFrame* aTargetFrame) = 0;

  /**
   * Get the type of reflow command.
   */
  NS_IMETHOD GetType(ReflowType& aReflowType) const = 0;

  /**
   * Get the child frame associated with the reflow command.
   */
  NS_IMETHOD GetChildFrame(nsIFrame*& aChildFrame) const = 0;

  /**
   * Returns the name of the child list to which the child frame belongs.
   * Only used for reflow command types FrameAppended, FrameInserted, and
   * FrameRemoved
   *
   * Returns nsnull if the child frame is associated with the unnamed
   * principal child list
   */
  NS_IMETHOD GetChildListName(nsIAtom*& aListName) const = 0;

  /**
   * Sets the name of the child list to which the child frame belongs.
   * Only used for reflow command types FrameAppended, FrameInserted, and
   * FrameRemoved
   */
  NS_IMETHOD SetChildListName(nsIAtom* aListName) = 0;

  /**
   * Get the previous sibling frame associated with the reflow command.
   * This is used for FrameInserted reflow commands.
   */
  NS_IMETHOD GetPrevSiblingFrame(nsIFrame*& aSiblingFrame) const = 0;

  /**
   * Dump out the reflow-command to out
   */
  NS_IMETHOD List(FILE* out) const = 0;

  /**
   * Get/set reflow command flags
   */
  NS_IMETHOD GetFlags(PRInt32* aFlags) = 0;
  NS_IMETHOD SetFlags(PRInt32 aFlags) = 0;
};

#endif /* nsIReflowCommand_h___ */
