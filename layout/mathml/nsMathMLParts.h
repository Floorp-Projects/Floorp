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

// Factory methods for creating MathML objects
nsIFrame* NS_NewMathMLTokenFrame(nsIPresShell* aPresShell, mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewMathMLmoFrame(nsIPresShell* aPresShell, mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewMathMLmrowFrame(nsIPresShell* aPresShell, mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewMathMLmpaddedFrame(nsIPresShell* aPresShell, mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewMathMLmspaceFrame(nsIPresShell* aPresShell, mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewMathMLmsFrame(nsIPresShell* aPresShell, mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewMathMLmfencedFrame(nsIPresShell* aPresShell, mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewMathMLmfracFrame(nsIPresShell* aPresShell, mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewMathMLmsubFrame(nsIPresShell* aPresShell, mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewMathMLmsupFrame(nsIPresShell* aPresShell, mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewMathMLmsubsupFrame(nsIPresShell* aPresShell, mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewMathMLmunderoverFrame(nsIPresShell* aPresShell, mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewMathMLmmultiscriptsFrame(nsIPresShell* aPresShell, mozilla::ComputedStyle* aStyle);
nsContainerFrame* NS_NewMathMLmtableOuterFrame(nsIPresShell* aPresShell, mozilla::ComputedStyle* aStyle);
nsContainerFrame* NS_NewMathMLmtableFrame(nsIPresShell* aPresShell, mozilla::ComputedStyle* aStyle);
nsContainerFrame* NS_NewMathMLmtrFrame(nsIPresShell* aPresShell, mozilla::ComputedStyle* aStyle);
nsContainerFrame* NS_NewMathMLmtdFrame(nsIPresShell* aPresShell, mozilla::ComputedStyle* aStyle, nsTableFrame* aTableFrame);
nsContainerFrame* NS_NewMathMLmtdInnerFrame(nsIPresShell* aPresShell, mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewMathMLmsqrtFrame(nsIPresShell* aPresShell, mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewMathMLmrootFrame(nsIPresShell* aPresShell, mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewMathMLmactionFrame(nsIPresShell* aPresShell, mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewMathMLmencloseFrame(nsIPresShell* aPresShell, mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewMathMLsemanticsFrame(nsIPresShell* aPresShell, mozilla::ComputedStyle* aStyle);

nsContainerFrame* NS_NewMathMLmathBlockFrame(nsIPresShell* aPresShell, mozilla::ComputedStyle* aStyle);
nsContainerFrame* NS_NewMathMLmathInlineFrame(nsIPresShell* aPresShell, mozilla::ComputedStyle* aStyle);
#endif /* nsMathMLParts_h___ */
