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

#include "nsDebug.h"
#include "PLDHashTable.h"
#include "mozilla/Attributes.h"

class nsIFrame;
class nsIPresShell;
class nsStyleSet;

class nsFrameManagerBase
{
public:
  nsFrameManagerBase();

  bool IsDestroyingFrames() { return mIsDestroyingFrames; }

  /*
   * Gets and sets the root frame (typically the viewport). The lifetime of the
   * root frame is controlled by the frame manager. When the frame manager is
   * destroyed, it destroys the entire frame hierarchy.
   */
  nsIFrame* GetRootFrame() const { return mRootFrame; }
  void      SetRootFrame(nsIFrame* aRootFrame)
  {
    NS_ASSERTION(!mRootFrame, "already have a root frame");
    mRootFrame = aRootFrame;
  }

  static uint32_t GetGlobalGenerationNumber() { return sGlobalGenerationNumber; }

protected:
  class UndisplayedMap;

  // weak link, because the pres shell owns us
  nsIPresShell* MOZ_NON_OWNING_REF mPresShell;
  nsIFrame*                       mRootFrame;
  PLDHashTable                    mPlaceholderMap;
  UndisplayedMap*                 mUndisplayedMap;
  UndisplayedMap*                 mDisplayContentsMap;
  bool                            mIsDestroyingFrames;  // The frame manager is destroying some frame(s).

  // The frame tree generation number
  // We use this to avoid unnecessary screenshotting
  // on Android. Unfortunately, this is static to match
  // the single consumer which is also static. Keeping
  // this the same greatly simplifies lifetime issues and
  // makes sure we always using the correct number.
  // A per PresContext generation number is available
  // via nsPresContext::GetDOMGeneration
  static uint32_t                 sGlobalGenerationNumber;
};

#endif
