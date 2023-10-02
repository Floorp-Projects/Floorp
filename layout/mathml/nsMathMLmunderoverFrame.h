/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLmunderoverFrame_h___
#define nsMathMLmunderoverFrame_h___

#include "mozilla/Attributes.h"
#include "nsIReflowCallback.h"
#include "nsMathMLContainerFrame.h"

namespace mozilla {
class PresShell;
}  // namespace mozilla

//
// <munderover> -- attach an underscript-overscript pair to a base
//

class nsMathMLmunderoverFrame final : public nsMathMLContainerFrame,
                                      public nsIReflowCallback {
 public:
  NS_DECL_FRAMEARENA_HELPERS(nsMathMLmunderoverFrame)

  friend nsIFrame* NS_NewMathMLmunderoverFrame(mozilla::PresShell* aPresShell,
                                               ComputedStyle* aStyle);

  nsresult Place(DrawTarget* aDrawTarget, bool aPlaceOrigin,
                 ReflowOutput& aDesiredSize) override;

  NS_IMETHOD InheritAutomaticData(nsIFrame* aParent) override;

  NS_IMETHOD TransmitAutomaticData() override;

  NS_IMETHOD UpdatePresentationData(uint32_t aFlagsValues,
                                    uint32_t aFlagsToUpdate) override;

  void Destroy(DestroyContext&) override;

  nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                            int32_t aModType) override;

  uint8_t ScriptIncrement(nsIFrame* aFrame) override;

  // nsIReflowCallback.
  bool ReflowFinished() override;
  void ReflowCallbackCanceled() override;

 protected:
  explicit nsMathMLmunderoverFrame(ComputedStyle* aStyle,
                                   nsPresContext* aPresContext)
      : nsMathMLContainerFrame(aStyle, aPresContext, kClassID),
        mIncrementUnder(false),
        mIncrementOver(false) {}

  virtual ~nsMathMLmunderoverFrame();

 private:
  // Helper to set the "increment script level" flag on the element belonging
  // to a child frame given by aChildIndex.
  //
  // When this flag is set, the style system will increment the scriptlevel for
  // the child element. This is needed for situations where the style system
  // cannot itself determine the scriptlevel (mfrac, munder, mover, munderover).
  //
  // This should be called during reflow.
  //
  // We set the flag and if it changed, we request appropriate restyling and
  // also queue a post-reflow callback to ensure that restyle and reflow happens
  // immediately after the current reflow.
  void SetIncrementScriptLevel(uint32_t aChildIndex, bool aIncrement);
  void SetPendingPostReflowIncrementScriptLevel();

  bool mIncrementUnder;
  bool mIncrementOver;

  struct SetIncrementScriptLevelCommand {
    uint32_t mChildIndex;
    bool mDoIncrement;
  };

  nsTArray<SetIncrementScriptLevelCommand>
      mPostReflowIncrementScriptLevelCommands;
};

#endif /* nsMathMLmunderoverFrame_h___ */
