/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:ts=8:et:sw=4:
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@fas.harvard.edu> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsIInspectorCSSUtils_h___
#define nsIInspectorCSSUtils_h___

#include "nsISupports.h"
#include "nsCSSProps.h"
class nsRuleNode;
class nsIStyleRule;
class nsIFrame;
struct nsRect;
class nsIPresShell;
class nsIContent;
class nsIStyleContext;

// 0252892b-419a-49f4-981d-4c6716099451
#define NS_IINSPECTORCSSUTILS_IID \
  { 0x0252892b, 0x419a, 0x49f4, \
    {0x98, 0x1d, 0x4c, 0x67, 0x16, 0x09, 0x94, 0x51 } }

// 7ef2f07f-6e34-410b-8336-88acd1cd16b7
#define NS_INSPECTORCSSUTILS_CID \
  { 0x7ef2f07f, 0x6e34, 0x410b, \
    {0x83, 0x36, 0x88, 0xac, 0xd1, 0xcd, 0x16, 0xb7 } }

class nsIInspectorCSSUtils : public nsISupports {
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IINSPECTORCSSUTILS_IID)

    // Hooks to nsCSSProps static methods from another library (the
    // AddRefTable and ReleaseTable should be handled by the
    // implementation of this interface).
    NS_IMETHOD LookupCSSProperty(const nsAString& aName, nsCSSProperty *aProp) = 0;

    // Hooks to inline methods on nsRuleNode that have trouble linking
    // on certain debug builds (MacOSX Mach-O with gcc).
    NS_IMETHOD GetRuleNodeParent(nsRuleNode *aNode, nsRuleNode **aParent) = 0;
    NS_IMETHOD GetRuleNodeRule(nsRuleNode *aNode, nsIStyleRule **aRule) = 0;
    NS_IMETHOD IsRuleNodeRoot(nsRuleNode *aNode, PRBool *aIsRoot) = 0;

    // Hooks to avoid having to use nsStyleCoord.
    NS_IMETHOD AdjustRectForMargins(nsIFrame* aFrame, nsRect& aRect) = 0;

    // Hooks to methods that need layout atoms (static vars in layout)
    NS_IMETHOD GetStyleContextForFrame(nsIFrame* aFrame,
                                       nsIStyleContext** aStyleContext) = 0;

    // Hooks to ::GetStyleData<T>(nsCOMPtr<nsIStyleContext>, const nsStyleText**)
    // and calls nsStyleText->WhiteSpaceIsSignificant();
    NS_IMETHOD IsWhiteSpaceSignificant(nsIStyleContext* aStyleContext,
                                       PRBool *aIsSignificant) = 0;
};

#endif /* nsIInspectorCSSUtils_h___ */
