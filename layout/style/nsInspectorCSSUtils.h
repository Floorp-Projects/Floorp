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

#ifndef nsInspectorCSSUtils_h___
#define nsInspectorCSSUtils_h___

#include "nsIInspectorCSSUtils.h"
#include "nsStyleContext.h"
#include "nsAutoPtr.h"

class nsIPresShell;

class nsInspectorCSSUtils : public nsIInspectorCSSUtils {

public:

    nsInspectorCSSUtils();
    virtual ~nsInspectorCSSUtils();

    NS_DECL_ISUPPORTS

    // nsIInspectorCSSUtils
    NS_IMETHOD LookupCSSProperty(const nsAString& aName, nsCSSProperty *aProp);
    NS_IMETHOD GetRuleNodeParent(nsRuleNode *aNode, nsRuleNode **aParent);
    NS_IMETHOD GetRuleNodeRule(nsRuleNode *aNode, nsIStyleRule **aRule);
    NS_IMETHOD IsRuleNodeRoot(nsRuleNode *aNode, PRBool *aIsRoot);
    NS_IMETHOD GetRuleNodeForContent(nsIContent* aContent,
                                     nsRuleNode** aRuleNode);
    NS_IMETHOD GetBindingURLs(nsIDOMElement *aElement, nsIArray **aResult);

    static already_AddRefed<nsStyleContext>
    GetStyleContextForContent(nsIContent* aContent, nsIAtom* aPseudo,
                              nsIPresShell* aPresShell);

private:
    static nsStyleContext* GetStyleContextForFrame(nsIFrame* aFrame);
};

#endif /* nsInspectorCSSUtils_h___ */
