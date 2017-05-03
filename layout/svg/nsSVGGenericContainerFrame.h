/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGGENERICCONTAINERFRAME_H__
#define __NS_SVGGENERICCONTAINERFRAME_H__

#include "mozilla/Attributes.h"
#include "gfxMatrix.h"
#include "nsFrame.h"
#include "nsLiteralString.h"
#include "nsQueryFrame.h"
#include "nsSVGContainerFrame.h"

class nsIAtom;
class nsIFrame;
class nsIPresShell;
class nsStyleContext;

class nsSVGGenericContainerFrame final : public nsSVGDisplayContainerFrame
{
  friend nsIFrame*
  NS_NewSVGGenericContainerFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

protected:
  explicit nsSVGGenericContainerFrame(nsStyleContext* aContext)
    : nsSVGDisplayContainerFrame(aContext,
                                 mozilla::LayoutFrameType::SVGGenericContainer)
  {}

public:
  NS_DECL_FRAMEARENA_HELPERS

  // nsIFrame:
  virtual nsresult  AttributeChanged(int32_t         aNameSpaceID,
                                     nsIAtom*        aAttribute,
                                     int32_t         aModType) override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGGenericContainer"), aResult);
  }
#endif

  // nsSVGContainerFrame methods:
  virtual gfxMatrix GetCanvasTM() override;
};

#endif // __NS_SVGGENERICCONTAINERFRAME_H__
