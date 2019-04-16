/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLParts_h___
#define nsMathMLParts_h___

#include "nscore.h"
#include "nsISupports.h"

class nsTableFrame;

namespace mozilla {
class PresShell;
}  // namespace mozilla

// Factory methods for creating MathML objects
nsIFrame* NS_NewMathMLTokenFrame(mozilla::PresShell* aPresShell,
                                 mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewMathMLmoFrame(mozilla::PresShell* aPresShell,
                              mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewMathMLmrowFrame(mozilla::PresShell* aPresShell,
                                mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewMathMLmpaddedFrame(mozilla::PresShell* aPresShell,
                                   mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewMathMLmspaceFrame(mozilla::PresShell* aPresShell,
                                  mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewMathMLmsFrame(mozilla::PresShell* aPresShell,
                              mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewMathMLmfencedFrame(mozilla::PresShell* aPresShell,
                                   mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewMathMLmfracFrame(mozilla::PresShell* aPresShell,
                                 mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewMathMLmsubFrame(mozilla::PresShell* aPresShell,
                                mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewMathMLmsupFrame(mozilla::PresShell* aPresShell,
                                mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewMathMLmsubsupFrame(mozilla::PresShell* aPresShell,
                                   mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewMathMLmunderoverFrame(mozilla::PresShell* aPresShell,
                                      mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewMathMLmmultiscriptsFrame(mozilla::PresShell* aPresShell,
                                         mozilla::ComputedStyle* aStyle);
nsContainerFrame* NS_NewMathMLmtableOuterFrame(mozilla::PresShell* aPresShell,
                                               mozilla::ComputedStyle* aStyle);
nsContainerFrame* NS_NewMathMLmtableFrame(mozilla::PresShell* aPresShell,
                                          mozilla::ComputedStyle* aStyle);
nsContainerFrame* NS_NewMathMLmtrFrame(mozilla::PresShell* aPresShell,
                                       mozilla::ComputedStyle* aStyle);
nsContainerFrame* NS_NewMathMLmtdFrame(mozilla::PresShell* aPresShell,
                                       mozilla::ComputedStyle* aStyle,
                                       nsTableFrame* aTableFrame);
nsContainerFrame* NS_NewMathMLmtdInnerFrame(mozilla::PresShell* aPresShell,
                                            mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewMathMLmsqrtFrame(mozilla::PresShell* aPresShell,
                                 mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewMathMLmrootFrame(mozilla::PresShell* aPresShell,
                                 mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewMathMLmactionFrame(mozilla::PresShell* aPresShell,
                                   mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewMathMLmencloseFrame(mozilla::PresShell* aPresShell,
                                    mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewMathMLsemanticsFrame(mozilla::PresShell* aPresShell,
                                     mozilla::ComputedStyle* aStyle);

nsContainerFrame* NS_NewMathMLmathBlockFrame(mozilla::PresShell* aPresShell,
                                             mozilla::ComputedStyle* aStyle);
nsContainerFrame* NS_NewMathMLmathInlineFrame(mozilla::PresShell* aPresShell,
                                              mozilla::ComputedStyle* aStyle);
#endif /* nsMathMLParts_h___ */
