/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGISYMBOLFRAME_H__
#define __NS_SVGISYMBOLFRAME_H__

#include "nsSVGViewportFrame.h"

class nsSVGSymbolFrame
 : public nsSVGViewportFrame
{
  friend nsIFrame*
  NS_NewSVGSymbolFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
protected:
  explicit nsSVGSymbolFrame(nsStyleContext* aContext)
    : nsSVGViewportFrame(aContext, kClassID)
  {
  }

public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsSVGSymbolFrame)

#ifdef DEBUG
  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;
#endif

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGSymbol"), aResult);
  }
#endif

};

#endif