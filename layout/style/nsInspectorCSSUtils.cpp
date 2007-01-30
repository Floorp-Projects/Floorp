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

#include "nsInspectorCSSUtils.h"
#include "nsIStyleRule.h"
#include "nsRuleNode.h"
#include "nsString.h"
#include "nsGkAtoms.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsAutoPtr.h"
#include "nsIFrame.h"
#include "nsStyleSet.h"
#include "nsXBLBinding.h"
#include "nsXBLPrototypeBinding.h"
#include "nsIDOMElement.h"
#include "nsIMutableArray.h"

nsInspectorCSSUtils::nsInspectorCSSUtils()
{
    nsCSSProps::AddRefTable();
}

nsInspectorCSSUtils::~nsInspectorCSSUtils()
{
    nsCSSProps::ReleaseTable();
}

NS_IMPL_ISUPPORTS1(nsInspectorCSSUtils, nsIInspectorCSSUtils)

NS_IMETHODIMP
nsInspectorCSSUtils::LookupCSSProperty(const nsAString& aName, nsCSSProperty *aProp)
{
    *aProp = nsCSSProps::LookupProperty(aName);
    return NS_OK;
}

NS_IMETHODIMP
nsInspectorCSSUtils::GetRuleNodeParent(nsRuleNode *aNode, nsRuleNode **aParent)
{
    *aParent = aNode->GetParent();
    return NS_OK;
}

NS_IMETHODIMP
nsInspectorCSSUtils::GetRuleNodeRule(nsRuleNode *aNode, nsIStyleRule **aRule)
{
    *aRule = aNode->GetRule();
    NS_IF_ADDREF(*aRule);
    return NS_OK;
}

NS_IMETHODIMP
nsInspectorCSSUtils::IsRuleNodeRoot(nsRuleNode *aNode, PRBool *aIsRoot)
{
    *aIsRoot = aNode->IsRoot();
    return NS_OK;
}

NS_IMETHODIMP
nsInspectorCSSUtils::AdjustRectForMargins(nsIFrame* aFrame, nsRect& aRect)
{
    const nsStyleMargin* margins = aFrame->GetStyleMargin();

    // adjust coordinates for margins
    nsStyleCoord coord;
    if (margins->mMargin.GetTopUnit() == eStyleUnit_Coord) {
        margins->mMargin.GetTop(coord);
        aRect.y -= coord.GetCoordValue();
        aRect.height += coord.GetCoordValue();
    }
    if (margins->mMargin.GetLeftUnit() == eStyleUnit_Coord) {
        margins->mMargin.GetLeft(coord);
        aRect.x -= coord.GetCoordValue();
        aRect.width += coord.GetCoordValue();
    }
    if (margins->mMargin.GetRightUnit() == eStyleUnit_Coord) {
        margins->mMargin.GetRight(coord);
        aRect.width += coord.GetCoordValue();
    }
    if (margins->mMargin.GetBottomUnit() == eStyleUnit_Coord) {
        margins->mMargin.GetBottom(coord);
        aRect.height += coord.GetCoordValue();
    }

    return NS_OK;
}

/* static */
nsStyleContext*
nsInspectorCSSUtils::GetStyleContextForFrame(nsIFrame* aFrame)
{
    nsStyleContext* styleContext = aFrame->GetStyleContext();
    if (!styleContext)
        return nsnull;

    /* For tables the primary frame is the "outer frame" but the style
     * rules are applied to the "inner frame".  Luckily, the "outer
     * frame" actually inherits style from the "inner frame" so we can
     * just move one level up in the style context hierarchy....
     */
    if (aFrame->GetType() == nsGkAtoms::tableOuterFrame)
        return styleContext->GetParent();

    return styleContext;
}    

/* static */
already_AddRefed<nsStyleContext>
nsInspectorCSSUtils::GetStyleContextForContent(nsIContent* aContent,
                                               nsIAtom* aPseudo,
                                               nsIPresShell* aPresShell)
{
    if (!aPseudo) {
        aPresShell->FlushPendingNotifications(Flush_StyleReresolves);
        nsIFrame* frame = aPresShell->GetPrimaryFrameFor(aContent);
        if (frame) {
            nsStyleContext* result = GetStyleContextForFrame(frame);
            // this function returns an addrefed style context
            if (result)
                result->AddRef();
            return result;
        }
    }

    // No frame has been created or we have a pseudo, so resolve the
    // style ourselves
    nsRefPtr<nsStyleContext> parentContext;
    nsIContent* parent = aPseudo ? aContent : aContent->GetParent();
    if (parent)
        parentContext = GetStyleContextForContent(parent, nsnull, aPresShell);

    nsPresContext *presContext = aPresShell->GetPresContext();
    if (!presContext)
        return nsnull;

    nsStyleSet *styleSet = aPresShell->StyleSet();

    if (!aContent->IsNodeOfType(nsINode::eELEMENT)) {
        NS_ASSERTION(!aPseudo, "Shouldn't have a pseudo for a non-element!");
        return styleSet->ResolveStyleForNonElement(parentContext);
    }

    if (aPseudo) {
        return styleSet->ResolvePseudoStyleFor(aContent, aPseudo, parentContext);
    }
    
    return styleSet->ResolveStyleFor(aContent, parentContext);
}

NS_IMETHODIMP
nsInspectorCSSUtils::GetRuleNodeForContent(nsIContent* aContent,
                                           nsRuleNode** aRuleNode)
{
    *aRuleNode = nsnull;

    nsIDocument* doc = aContent->GetDocument();
    NS_ENSURE_TRUE(doc, NS_ERROR_UNEXPECTED);

    nsIPresShell *presShell = doc->GetShellAt(0);
    NS_ENSURE_TRUE(presShell, NS_ERROR_UNEXPECTED);

    nsRefPtr<nsStyleContext> sContext =
        GetStyleContextForContent(aContent, nsnull, presShell);
    *aRuleNode = sContext->GetRuleNode();
    return NS_OK;
}

NS_IMETHODIMP
nsInspectorCSSUtils::GetBindingURLs(nsIDOMElement *aElement,
                                    nsIArray **aResult)
{
    *aResult = nsnull;

    nsCOMPtr<nsIMutableArray> urls = do_CreateInstance(NS_ARRAY_CONTRACTID);
    if (!urls)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
    NS_ASSERTION(content, "elements must implement nsIContent");

    nsIDocument *ownerDoc = content->GetOwnerDoc();
    if (ownerDoc) {
        nsXBLBinding *binding =
            ownerDoc->BindingManager()->GetBinding(content);

        while (binding) {
            urls->AppendElement(binding->PrototypeBinding()->BindingURI(),
                                PR_FALSE);
            binding = binding->GetBaseBinding();
        }
    }

    NS_ADDREF(*aResult = urls);
    return NS_OK;
}
