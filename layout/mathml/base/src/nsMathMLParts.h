/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla MathML Project.
 * 
 * The Initial Developer of the Original Code is The University Of 
 * Queensland.  Portions created by The University Of Queensland are
 * Copyright (C) 1999 The University Of Queensland.  All Rights Reserved.
 * 
 * Contributor(s): 
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
 */

#ifndef nsMathMLParts_h___
#define nsMathMLParts_h___

#include "nscore.h"
#include "nsISupports.h"

// Factory methods for creating MathML objects
extern nsresult NS_NewMathMLmrowFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );
extern nsresult NS_NewMathMLmiFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );
extern nsresult NS_NewMathMLmtextFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );
extern nsresult NS_NewMathMLmoFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );
extern nsresult NS_NewMathMLmphantomFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );
extern nsresult NS_NewMathMLmpaddedFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );
extern nsresult NS_NewMathMLmspaceFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );
extern nsresult NS_NewMathMLmsFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );
extern nsresult NS_NewMathMLmfencedFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );
extern nsresult NS_NewMathMLmfracFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );
extern nsresult NS_NewMathMLmsubFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );
extern nsresult NS_NewMathMLmsupFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );
extern nsresult NS_NewMathMLmsubsupFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );
extern nsresult NS_NewMathMLmunderFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );
extern nsresult NS_NewMathMLmoverFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );
extern nsresult NS_NewMathMLmunderoverFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );
extern nsresult NS_NewMathMLmmultiscriptsFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );
extern nsresult NS_NewMathMLmstyleFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );
extern nsresult NS_NewMathMLmtableOuterFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );
extern nsresult NS_NewMathMLmtdFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame );
extern nsresult NS_NewMathMLmsqrtFrame( nsIPresShell* aPresShell, nsIFrame** aNewFrame );
extern nsresult NS_NewMathMLmrootFrame( nsIPresShell* aPresShell, nsIFrame** aNewFrame );
extern nsresult NS_NewMathMLmactionFrame( nsIPresShell* aPresShell, nsIFrame** aNewFrame );

#endif /* nsMathMLParts_h___ */
