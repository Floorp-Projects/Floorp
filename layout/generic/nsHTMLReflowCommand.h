/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsHTMLReflowCommand_h___
#define nsHTMLReflowCommand_h___
#include "nsReflowType.h"
#include "nsVoidArray.h"
#include <stdio.h>
#include "nsIAtom.h"

class  nsIFrame;
class  nsPresContext;
class  nsIRenderingContext;
struct nsHTMLReflowMetrics;
struct nsSize;

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
class nsHTMLReflowCommand {
public:
  /**
   * Construct an HTML reflow command of type aReflowType and with target
   * frame aTargetFrame. You can also specify an optional child frame, e.g.
   * to indicate the inserted child frame
   */
  nsHTMLReflowCommand(nsIFrame*    aTargetFrame,
                      nsReflowType aReflowType,
                      nsIFrame*    aChildFrame = nsnull,
                      nsIAtom*     aAttribute = nsnull);

  ~nsHTMLReflowCommand();

  /**
   * Get the target of the reflow command.
   */
  nsIFrame* GetTarget() const { return mTargetFrame; }

  /**
   * Get the type of reflow command.
   */
  nsReflowType GetType() const { return mType; }

  /**
   * Can return nsnull.
   */
  nsIAtom* GetAttribute() const { return mAttribute; }

  /**
   * Get the child frame associated with the reflow command.
   */
  nsIFrame* GetChildFrame() const { return mChildFrame; }

  /**
   * Returns the name of the child list to which the child frame belongs.
   * Only used for reflow command types FrameAppended, FrameInserted, and
   * FrameRemoved
   *
   * Returns nsnull if the child frame is associated with the unnamed
   * principal child list
   */
  nsIAtom* GetChildListName() const { return mListName; }

  /**
   * Sets the name of the child list to which the child frame belongs.
   * Only used for reflow command types FrameAppended, FrameInserted, and
   * FrameRemoved
   */
  void SetChildListName(nsIAtom* aListName) {
    NS_ASSERTION(!mListName, "SetChildListName called twice");
    mListName = aListName;
    NS_IF_ADDREF(mListName);
  }

  /**
   * Dump out the reflow-command to out
   */
  nsresult List(FILE* out) const;

  /**
   * Get/set reflow command flags
   */
  PRInt32 GetFlagBits() { return mFlags; }
  void AddFlagBits(PRInt32 aBits) { mFlags |= aBits; }
  void RemoveFlagBits(PRInt32 aBits) { mFlags &= ~aBits; }

  /**
   * DEPRECATED compatibility methods
   */
  nsresult GetTarget(nsIFrame*& aTargetFrame) const {
    aTargetFrame = mTargetFrame;
    return NS_OK;
  }

  nsresult GetType(nsReflowType& aReflowType) const {
    aReflowType = mType;
    return NS_OK;
  }

  nsresult GetAttribute(nsIAtom *& aAttribute) const {
    aAttribute = mAttribute;
    NS_IF_ADDREF(aAttribute);
    return NS_OK;
  }

  nsresult GetChildFrame(nsIFrame*& aChildFrame) const {
    aChildFrame = mChildFrame;
    return NS_OK;
  }

  nsresult GetChildListName(nsIAtom*& aListName) const {
    aListName = mListName;
    NS_IF_ADDREF(aListName);
    return NS_OK;
  }

  nsresult GetFlags(PRInt32* aFlags) {
    *aFlags = mFlags;
    return NS_OK;
  }

  nsresult SetFlags(PRInt32 aFlags) {
    mFlags = aFlags;
    return NS_OK;
  }

private:
  nsReflowType    mType;
  nsIFrame*       mTargetFrame;
  nsIFrame*       mChildFrame;
  nsIAtom*        mAttribute;
  nsIAtom*        mListName;
  PRInt32         mFlags;
};

#endif /* nsHTMLReflowCommand_h___ */
