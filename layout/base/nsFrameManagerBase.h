/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* part of nsFrameManager, to work around header inclusionordering */

#ifndef _nsFrameManagerBase_h_
#define _nsFrameManagerBase_h_

#include "nsDebug.h"
#include "PLDHashTable.h"
#include "mozilla/Attributes.h"

class nsIFrame;
class nsIPresShell;

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

protected:
  class UndisplayedMap;

  // weak link, because the pres shell owns us
  nsIPresShell* MOZ_NON_OWNING_REF mPresShell;
  nsIFrame*                       mRootFrame;
  UndisplayedMap*                 mDisplayNoneMap;
  UndisplayedMap*                 mDisplayContentsMap;
  bool                            mIsDestroyingFrames;  // The frame manager is destroying some frame(s).
};

#endif
