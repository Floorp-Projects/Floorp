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
#ifndef nsHTMLReflowCommand_h___
#define nsHTMLReflowCommand_h___
#include "nsReflowType.h"
#include "nsVoidArray.h"

class  nsIAtom;
class  nsIFrame;
class  nsIPresContext;
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
   * Dispatch the reflow command.
   *
   * Builds a path from the target frame back to the root frame, and then
   * invokes the root frame's Reflow() member function.
   *
   * @see nsIFrame#Reflow()
   */
  nsresult Dispatch(nsIPresContext*      aPresContext,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsSize&        aMaxSize,
                    nsIRenderingContext& aRendContext);

  /**
   * Get the next frame in the command processing path. If requested removes the
   * the frame from the path. You must remove the frame from the path before
   * dispatching the reflow command to the next frame in the chain.
   */
  nsresult GetNext(nsIFrame*& aNextFrame, PRBool aRemove = PR_TRUE);

  /**
   * Get the target of the reflow command.
   */
  nsresult GetTarget(nsIFrame*& aTargetFrame) const;

  /**
   * Change the target of the reflow command.
   */
  nsresult SetTarget(nsIFrame* aTargetFrame);

  /**
   * Get the type of reflow command.
   */
  nsresult GetType(nsReflowType& aReflowType) const;

  /**
   * Can return nsnull.  If nsnull is not returned, the caller must NS_RELEASE aAttribute
   */
  nsresult GetAttribute(nsIAtom *& aAttribute) const;

  /**
   * Return the reflow command's path. The path is stored in
   * <em>reverse</em> order in the array; i.e., the first element in
   * the array is the target frame, the last element in the array is
   * the current frame.
   */
  nsVoidArray *GetPath() { return &mPath; }

  /**
   * Get the child frame associated with the reflow command.
   */
  nsresult GetChildFrame(nsIFrame*& aChildFrame) const;

  /**
   * Returns the name of the child list to which the child frame belongs.
   * Only used for reflow command types FrameAppended, FrameInserted, and
   * FrameRemoved
   *
   * Returns nsnull if the child frame is associated with the unnamed
   * principal child list
   */
  nsresult GetChildListName(nsIAtom*& aListName) const;

  /**
   * Sets the name of the child list to which the child frame belongs.
   * Only used for reflow command types FrameAppended, FrameInserted, and
   * FrameRemoved
   */
  nsresult SetChildListName(nsIAtom* aListName);

  /**
   * Get the previous sibling frame associated with the reflow command.
   * This is used for FrameInserted reflow commands.
   */
  nsresult GetPrevSiblingFrame(nsIFrame*& aSiblingFrame) const;

  /**
   * Dump out the reflow-command to out
   */
  nsresult List(FILE* out) const;

  /**
   * Get/set reflow command flags
   */
  nsresult GetFlags(PRInt32* aFlags);
  nsresult SetFlags(PRInt32 aFlags);

protected:
  void      BuildPath();

private:
  nsReflowType    mType;
  nsIFrame*       mTargetFrame;
  nsIFrame*       mChildFrame;
  nsIFrame*       mPrevSiblingFrame;
  nsIAtom*        mAttribute;
  nsIAtom*        mListName;
  nsAutoVoidArray mPath;
  PRInt32         mFlags;
};

#endif /* nsHTMLReflowCommand_h___ */
