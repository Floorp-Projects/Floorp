/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla MathML Project.
 *
 * The Initial Developer of the Original Code is
 * The University Of Queensland.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsMathMLParts_h___
#define nsMathMLParts_h___

#include "nscore.h"
#include "nsISupports.h"

// Factory methods for creating MathML objects
nsIFrame* NS_NewMathMLForeignFrameWrapper(nsIPresShell* aPresShell, nsStyleContext* aContext);
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
nsIFrame* NS_NewMathMLmstyleFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
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

nsIFrame* NS_NewMathMLmathBlockFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, PRUint32 aFlags);
nsIFrame* NS_NewMathMLmathInlineFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
inline nsIFrame* NS_CreateNewMathMLmathBlockFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return NS_NewMathMLmathBlockFrame(aPresShell, aContext, 0);
}
#endif /* nsMathMLParts_h___ */
