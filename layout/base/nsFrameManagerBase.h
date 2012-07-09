/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:cindent:ts=2:et:sw=2:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This Original Code has been modified by IBM Corporation. Modifications made
 * by IBM described herein are Copyright (c) International Business Machines
 * Corporation, 2000. Modifications to Mozilla code or documentation identified
 * per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      OS/2 VisualAge build.
 */

/* part of nsFrameManager, to work around header inclusionordering */

#ifndef _nsFrameManagerBase_h_
#define _nsFrameManagerBase_h_

#include "pldhash.h"

class nsIPresShell;
class nsStyleSet;
class nsIContent;
class nsPlaceholderFrame;
class nsIFrame;
class nsStyleContext;
class nsIAtom;
class nsStyleChangeList;
class nsILayoutHistoryState;

class nsFrameManagerBase
{
public:
  nsFrameManagerBase()
  {
    memset(this, '\0', sizeof(nsFrameManagerBase));
  }

  bool IsDestroyingFrames() { return mIsDestroyingFrames; }

  /*
   * Gets and sets the root frame (typically the viewport). The lifetime of the
   * root frame is controlled by the frame manager. When the frame manager is
   * destroyed, it destroys the entire frame hierarchy.
   */
  NS_HIDDEN_(nsIFrame*) GetRootFrame() const { return mRootFrame; }
  NS_HIDDEN_(void)      SetRootFrame(nsIFrame* aRootFrame)
  {
    NS_ASSERTION(!mRootFrame, "already have a root frame");
    mRootFrame = aRootFrame;
  }

  static PRUint32 GetGlobalGenerationNumber() { return sGlobalGenerationNumber; }

protected:
  class UndisplayedMap;

  // weak link, because the pres shell owns us
  nsIPresShell*                   mPresShell;
  // the pres shell owns the style set
  nsStyleSet*                     mStyleSet;
  nsIFrame*                       mRootFrame;
  PLDHashTable                    mPlaceholderMap;
  UndisplayedMap*                 mUndisplayedMap;
  bool                            mIsDestroyingFrames;  // The frame manager is destroying some frame(s).

  // The frame tree generation number
  // We use this to avoid unnecessary screenshotting
  // on Android. Unfortunately, this is static to match
  // the single consumer which is also static. Keeping
  // this the same greatly simplifies lifetime issues and
  // makes sure we always using the correct number.
  // A per PresContext generation number is available
  // via nsPresContext::GetDOMGeneration
  static PRUint32                 sGlobalGenerationNumber;
};

#endif
