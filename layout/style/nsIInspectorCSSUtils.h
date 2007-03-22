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
 *   L. David Baron <dbaron@dbaron.org> (original author)
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

/* XPCOM interface to provide some internal information to DOM inspector */

#ifndef nsIInspectorCSSUtils_h___
#define nsIInspectorCSSUtils_h___

#include "nsISupports.h"
#include "nsCSSProps.h"
class nsRuleNode;
class nsIStyleRule;
class nsIFrame;
struct nsRect;
class nsIContent;
class nsIDOMElement;
class nsIArray;

// 35dfc2a6-b069-4014-ad4b-01927e77d828
#define NS_IINSPECTORCSSUTILS_IID \
  { 0x35dfc2a6, 0xb069, 0x4014, \
    {0xad, 0x4b, 0x01, 0x92, 0x7e, 0x77, 0xd8, 0x28 } }

// 7ef2f07f-6e34-410b-8336-88acd1cd16b7
#define NS_INSPECTORCSSUTILS_CID \
  { 0x7ef2f07f, 0x6e34, 0x410b, \
    {0x83, 0x36, 0x88, 0xac, 0xd1, 0xcd, 0x16, 0xb7 } }

class nsIInspectorCSSUtils : public nsISupports {
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_IINSPECTORCSSUTILS_IID)

    // Hooks to nsCSSProps static methods from another library (the
    // AddRefTable and ReleaseTable should be handled by the
    // implementation of this interface).
    NS_IMETHOD LookupCSSProperty(const nsAString& aName, nsCSSProperty *aProp) = 0;

    // Hooks to inline methods on nsRuleNode that have trouble linking
    // on certain debug builds (MacOSX Mach-O with gcc).
    NS_IMETHOD GetRuleNodeParent(nsRuleNode *aNode, nsRuleNode **aParent) = 0;
    NS_IMETHOD GetRuleNodeRule(nsRuleNode *aNode, nsIStyleRule **aRule) = 0;
    NS_IMETHOD IsRuleNodeRoot(nsRuleNode *aNode, PRBool *aIsRoot) = 0;

    // Hooks to methods that need nsStyleContext
    NS_IMETHOD GetRuleNodeForContent(nsIContent* aContent,
                                     nsRuleNode** aParent) = 0;

    // Hooks to XBL
    NS_IMETHOD GetBindingURLs(nsIDOMElement *aElement, nsIArray **aResult) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIInspectorCSSUtils, NS_IINSPECTORCSSUTILS_IID)

#endif /* nsIInspectorCSSUtils_h___ */
