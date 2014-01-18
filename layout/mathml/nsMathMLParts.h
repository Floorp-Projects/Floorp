/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLParts_h___
#define nsMathMLParts_h___

#include "nscore.h"
#include "nsISupports.h"

// Factory methods for creating MathML objects
nsIFrame* NS_NewMathMLTokenFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame* NS_NewMathMLmoFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame* NS_NewMathMLmrowFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame* NS_NewMathMLmphantomFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame* NS_NewMathMLmpaddedFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame* NS_NewMathMLmspaceFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame* NS_NewMathMLmsFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame* NS_NewMathMLmfencedFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame* NS_NewMathMLmfracFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame* NS_NewMathMLmsubFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame* NS_NewMathMLmsupFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame* NS_NewMathMLmsubsupFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame* NS_NewMathMLmunderoverFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame* NS_NewMathMLmmultiscriptsFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame* NS_NewMathMLmtableOuterFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame* NS_NewMathMLmtableFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame* NS_NewMathMLmtrFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame* NS_NewMathMLmtdFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame* NS_NewMathMLmtdInnerFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame* NS_NewMathMLmsqrtFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame* NS_NewMathMLmrootFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame* NS_NewMathMLmactionFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame* NS_NewMathMLmencloseFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame* NS_NewMathMLsemanticsFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

nsIFrame* NS_NewMathMLmathBlockFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, uint32_t aFlags);
nsIFrame* NS_NewMathMLmathInlineFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
inline nsIFrame* NS_CreateNewMathMLmathBlockFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return NS_NewMathMLmathBlockFrame(aPresShell, aContext, 0);
}
#endif /* nsMathMLParts_h___ */
