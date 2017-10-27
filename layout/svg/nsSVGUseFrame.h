/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGUSEFRAME_H__
#define __NS_SVGUSEFRAME_H__

// Keep in (case-insensitive) order:
#include "nsIAnonymousContentCreator.h"
#include "nsSVGGFrame.h"

class nsSVGUseFrame final
  : public nsSVGGFrame
  , public nsIAnonymousContentCreator
{
  friend nsIFrame* NS_NewSVGUseFrame(nsIPresShell* aPresShell,
                                     nsStyleContext* aContext);

protected:
  explicit nsSVGUseFrame(nsStyleContext* aContext)
    : nsSVGGFrame(aContext, kClassID)
    , mHasValidDimensions(true)
  {
  }

public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsSVGUseFrame)

  // nsIFrame interface:
  void Init(nsIContent* aContent,
            nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;

  nsresult AttributeChanged(int32_t aNameSpaceID,
                            nsAtom* aAttribute,
                            int32_t aModType) override;

  void DestroyFrom(nsIFrame* aDestructRoot) override;

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGUse"), aResult);
  }
#endif

  // nsSVGDisplayableFrame interface:
  void ReflowSVG() override;
  void NotifySVGChanged(uint32_t aFlags) override;

  // nsIAnonymousContentCreator
  nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements) override;
  void AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                uint32_t aFilter) override;

  nsIContent* GetContentClone() { return mContentClone.get(); }

private:
  bool mHasValidDimensions;
  nsCOMPtr<nsIContent> mContentClone;
};

#endif // __NS_SVGUSEFRAME_H__
