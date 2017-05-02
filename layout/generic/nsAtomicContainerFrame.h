/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* base class for rendering objects that need child lists but behave like leaf */

#ifndef nsAtomicContainerFrame_h___
#define nsAtomicContainerFrame_h___

#include "nsContainerFrame.h"

/**
 * This class is for frames which need child lists but act like a leaf
 * frame. In general, all frames of elements laid out according to the
 * CSS box model would need child list for ::backdrop in case they are
 * in fullscreen, while some of them still want leaf frame behavior.
 */
class nsAtomicContainerFrame : public nsContainerFrame
{
public:
  NS_DECL_ABSTRACT_FRAME(nsAtomicContainerFrame)

  // Bypass the nsContainerFrame/nsSplittableFrame impl of the following
  // methods so we behave like a leaf frame.
  bool IsLeaf() const override { return true; }
  FrameSearchResult PeekOffsetNoAmount(bool aForward, int32_t* aOffset) override
  {
    return nsFrame::PeekOffsetNoAmount(aForward, aOffset);
  }
  FrameSearchResult PeekOffsetCharacter(bool aForward, int32_t* aOffset,
                                        bool aRespectClusters = true) override
  {
    return nsFrame::PeekOffsetCharacter(aForward, aOffset, aRespectClusters);
  }
  nsSplittableType GetSplittableType() const override
  {
    return nsFrame::GetSplittableType();
  }

protected:
  nsAtomicContainerFrame(nsStyleContext* aContext, mozilla::FrameType aType)
    : nsContainerFrame(aContext, aType)
  {}
};

#endif // nsAtomicContainerFrame_h___
