/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:cindent:ts=2:et:sw=2:
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
 * ***** END LICENSE BLOCK *****
 *
 * This Original Code has been modified by IBM Corporation. Modifications made
 * by IBM described herein are Copyright (c) International Business Machines
 * Corporation, 2000. Modifications to Mozilla code or documentation identified
 * per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      OS/2 VisualAge build.
 */

/* storage of the frame tree and information about it */

#ifndef _nsFrameManager_h_
#define _nsFrameManager_h_

#include "nsIFrame.h"
#include "nsIStatefulFrame.h"
#include "nsChangeHint.h"
#include "nsFrameManagerBase.h"

namespace mozilla {
namespace css {
class RestyleTracker;
} // namespace css
} // namespace mozilla

struct TreeMatchContext;

/**
 * Frame manager interface. The frame manager serves two purposes:
 * <li>provides a service for mapping from content to frame and from
 * out-of-flow frame to placeholder frame.
 * <li>handles structural modifications to the frame model. If the frame model
 * lock can be acquired, then the changes are processed immediately; otherwise,
 * they're queued and processed later.
 *
 * Do not add virtual methods to this class, or bryner will punish you.
 */

class nsFrameManager : public nsFrameManagerBase
{
  typedef mozilla::css::RestyleTracker RestyleTracker;
  typedef nsIFrame::ChildListID ChildListID;

public:
  nsFrameManager(nsIPresShell *aPresShell) NS_HIDDEN {
    mPresShell = aPresShell;
  }
  ~nsFrameManager() NS_HIDDEN;

  // Initialization
  NS_HIDDEN_(nsresult) Init(nsStyleSet* aStyleSet);

  /*
   * After Destroy is called, it is an error to call any FrameManager methods.
   * Destroy should be called when the frame tree managed by the frame
   * manager is no longer being displayed.
   */
  NS_HIDDEN_(void) Destroy();

  // Placeholder frame functions
  NS_HIDDEN_(nsPlaceholderFrame*) GetPlaceholderFrameFor(const nsIFrame* aFrame);
  NS_HIDDEN_(nsresult)
    RegisterPlaceholderFrame(nsPlaceholderFrame* aPlaceholderFrame);

  NS_HIDDEN_(void)
    UnregisterPlaceholderFrame(nsPlaceholderFrame* aPlaceholderFrame);

  NS_HIDDEN_(void)      ClearPlaceholderFrameMap();

  // Mapping undisplayed content
  NS_HIDDEN_(nsStyleContext*) GetUndisplayedContent(nsIContent* aContent);
  NS_HIDDEN_(void) SetUndisplayedContent(nsIContent* aContent,
                                         nsStyleContext* aStyleContext);
  NS_HIDDEN_(void) ChangeUndisplayedContent(nsIContent* aContent,
                                            nsStyleContext* aStyleContext);
  NS_HIDDEN_(void) ClearUndisplayedContentIn(nsIContent* aContent,
                                             nsIContent* aParentContent);
  NS_HIDDEN_(void) ClearAllUndisplayedContentIn(nsIContent* aParentContent);

  // Functions for manipulating the frame model
  NS_HIDDEN_(nsresult) AppendFrames(nsIFrame*       aParentFrame,
                                    ChildListID     aListID,
                                    nsFrameList&    aFrameList);

  NS_HIDDEN_(nsresult) InsertFrames(nsIFrame*       aParentFrame,
                                    ChildListID     aListID,
                                    nsIFrame*       aPrevFrame,
                                    nsFrameList&    aFrameList);

  NS_HIDDEN_(nsresult) RemoveFrame(ChildListID     aListID,
                                   nsIFrame*       aOldFrame,
                                   bool            aInvalidate = true);

  /*
   * Notification that a frame is about to be destroyed. This allows any
   * outstanding references to the frame to be cleaned up.
   */
  NS_HIDDEN_(void)     NotifyDestroyingFrame(nsIFrame* aFrame);

  /*
   * Reparent the style contexts of this frame subtree.  The parent frame of
   * aFrame must be changed to the new parent before this function is called;
   * the new parent style context will be automatically computed based on the
   * new position in the frame tree.
   *
   * @param aFrame the root of the subtree to reparent.  Must not be null.
   */
  NS_HIDDEN_(nsresult) ReparentStyleContext(nsIFrame* aFrame);

  /*
   * Re-resolve the style contexts for a frame tree, building
   * aChangeList based on the resulting style changes, plus aMinChange
   * applied to aFrame.
   */
  NS_HIDDEN_(void)
    ComputeStyleChangeFor(nsIFrame* aFrame,
                          nsStyleChangeList* aChangeList,
                          nsChangeHint aMinChange,
                          RestyleTracker& aRestyleTracker,
                          bool aRestyleDescendants);

  /*
   * Capture/restore frame state for the frame subtree rooted at aFrame.
   * aState is the document state storage object onto which each frame
   * stores its state.
   */

  NS_HIDDEN_(void) CaptureFrameState(nsIFrame*              aFrame,
                                     nsILayoutHistoryState* aState);

  NS_HIDDEN_(void) RestoreFrameState(nsIFrame*              aFrame,
                                     nsILayoutHistoryState* aState);

  /*
   * Add/restore state for one frame
   * (special, global type, like scroll position)
   */
  NS_HIDDEN_(void) CaptureFrameStateFor(nsIFrame*              aFrame,
                                        nsILayoutHistoryState* aState,
                                        nsIStatefulFrame::SpecialStateID aID =
                                                      nsIStatefulFrame::eNoID);

  NS_HIDDEN_(void) RestoreFrameStateFor(nsIFrame*              aFrame,
                                        nsILayoutHistoryState* aState,
                                        nsIStatefulFrame::SpecialStateID aID =
                                                      nsIStatefulFrame::eNoID);

#ifdef NS_DEBUG
  /**
   * DEBUG ONLY method to verify integrity of style tree versus frame tree
   */
  NS_HIDDEN_(void) DebugVerifyStyleTree(nsIFrame* aFrame);
#endif

  NS_HIDDEN_(nsIPresShell*) GetPresShell() const { return mPresShell; }
  NS_HIDDEN_(nsPresContext*) GetPresContext() const {
    return mPresShell->GetPresContext();
  }

private:
  enum DesiredA11yNotifications {
    eSkipNotifications,
    eSendAllNotifications,
    eNotifyIfShown
  };

  enum A11yNotificationType {
    eDontNotify,
    eNotifyShown,
    eNotifyHidden
  };

  // Use eRestyle_Self for the aRestyleHint argument to mean
  // "reresolve our style context but not kids", use eRestyle_Subtree
  // to mean "reresolve our style context and kids", and use
  // nsRestyleHint(0) to mean recompute a new style context for our
  // current parent and existing rulenode, and the same for kids.
  NS_HIDDEN_(nsChangeHint)
    ReResolveStyleContext(nsPresContext    *aPresContext,
                          nsIFrame          *aFrame,
                          nsIContent        *aParentContent,
                          nsStyleChangeList *aChangeList, 
                          nsChangeHint       aMinChange,
                          nsRestyleHint      aRestyleHint,
                          RestyleTracker&    aRestyleTracker,
                          DesiredA11yNotifications aDesiredA11yNotifications,
                          nsTArray<nsIContent*>& aVisibleKidsOfHiddenElement,
                          TreeMatchContext &aTreeMatchContext);
};

#endif
