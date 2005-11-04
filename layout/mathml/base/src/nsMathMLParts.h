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
nsresult
NS_NewMathMLForeignFrameWrapper(nsIPresShell* aPresShell,
                                nsIFrame** aNewFrame);
nsresult
NS_NewMathMLTokenFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);
nsresult
NS_NewMathMLmoFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);
nsresult
NS_NewMathMLmrowFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);
nsresult
NS_NewMathMLmphantomFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);
nsresult
NS_NewMathMLmpaddedFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);
nsresult
NS_NewMathMLmspaceFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);
nsresult
NS_NewMathMLmsFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);
nsresult
NS_NewMathMLmfencedFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);
nsresult
NS_NewMathMLmfracFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);
nsresult
NS_NewMathMLmsubFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);
nsresult
NS_NewMathMLmsupFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);
nsresult
NS_NewMathMLmsubsupFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);
nsresult
NS_NewMathMLmunderFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);
nsresult
NS_NewMathMLmoverFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);
nsresult
NS_NewMathMLmunderoverFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);
nsresult
NS_NewMathMLmmultiscriptsFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);
nsresult
NS_NewMathMLmstyleFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

nsIFrame*
NS_NewMathMLmtableOuterFrame(nsIPresShell* aPresShell);
inline nsIFrame*
NS_NewMathMLmtableFrame(nsIPresShell* aPresShell)
{
  return NS_NewTableFrame(aPresShell);
}
inline nsIFrame*
NS_NewMathMLmtrFrame(nsIPresShell* aPresShell)
{
  return NS_NewTableRowFrame(aPresShell);
}
nsIFrame*
NS_NewMathMLmtdFrame(nsIPresShell* aPresShell);
nsIFrame*
NS_NewMathMLmtdInnerFrame(nsIPresShell* aPresShell);
nsresult
NS_NewMathMLmsqrtFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);
nsresult
NS_NewMathMLmrootFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);
nsresult
NS_NewMathMLmactionFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

nsresult
NS_NewMathMLmathBlockFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);
nsresult
NS_NewMathMLmathInlineFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);
inline nsresult
NS_NewMathMLmathFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame,
                      PRBool aIsBlock)
{
  return (aIsBlock)
    ? NS_NewMathMLmathBlockFrame(aPresShell, aNewFrame)
    : NS_NewMathMLmathInlineFrame(aPresShell, aNewFrame);
}
#endif /* nsMathMLParts_h___ */
