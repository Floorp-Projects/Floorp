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

#include "nsInspectorCSSUtils.h"
#include "nsRuleNode.h"
#include "nsString.h"
#include "nsLayoutAtoms.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"

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
    return aNode->GetRule(aRule);
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
  const nsStyleMargin* margins;
  ::GetStyleData(aFrame, &margins);
  
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

nsresult
nsInspectorCSSUtils::GetStyleContextForFrame(nsIFrame* aFrame,
                                             nsIStyleContext** aStyleContext)
{
    nsCOMPtr<nsIStyleContext> styleContext;
    aFrame->GetStyleContext(getter_AddRefs(styleContext));
    if (!styleContext) {
        // Caller returns rv on through, and this does not seem
        // exception-worthy.
        *aStyleContext = nsnull;
        return NS_OK;
    }

    /* For tables the primary frame is the "outer frame" but the style
     * rules are applied to the "inner frame".  Luckily, the "outer
     * frame" actually inherits style from the "inner frame" so we can
     * just move one level up in the style context hierarchy....
     */
    nsCOMPtr<nsIAtom> frameType;
    aFrame->GetFrameType(getter_AddRefs(frameType));
    if (frameType == nsLayoutAtoms::tableOuterFrame) {
        *aStyleContext = styleContext->GetParent().get();
    } else {
        *aStyleContext = styleContext;
        NS_ADDREF(*aStyleContext);
    }
    return NS_OK;
}    

nsresult
nsInspectorCSSUtils::GetStyleContextForContent(nsIContent* aContent,
                                               nsIPresShell* aPresShell,
                                               nsIStyleContext** aStyleContext)
{
    nsIFrame* frame = nsnull;
    aPresShell->GetPrimaryFrameFor(aContent, &frame);
    if (frame)
        return GetStyleContextForFrame(frame, aStyleContext);

    // No frame has been created, so resolve the style ourself.
    nsCOMPtr<nsIStyleContext> parentContext;
    nsCOMPtr<nsIContent> parent;
    aContent->GetParent(*getter_AddRefs(parent));
    if (parent) {
        nsresult rv = GetStyleContextForContent(aContent, aPresShell,
                                                getter_AddRefs(parentContext));
        NS_ENSURE_SUCCESS(rv, rv);
    }

    nsCOMPtr<nsIPresContext> presContext;
    aPresShell->GetPresContext(getter_AddRefs(presContext));
    NS_ENSURE_TRUE(presContext, NS_ERROR_UNEXPECTED);

    if (aContent->IsContentOfType(nsIContent::eELEMENT))
        return presContext->ResolveStyleContextFor(aContent, parentContext,
                                                   aStyleContext);

    return presContext->ResolveStyleContextForNonElement(parentContext,
                                                         aStyleContext);
}

NS_IMETHODIMP
nsInspectorCSSUtils::GetRuleNodeForContent(nsIContent* aContent,
                                           nsRuleNode** aRuleNode)
{
    *aRuleNode = nsnull;

    nsCOMPtr<nsIDocument> doc;
    aContent->GetDocument(*getter_AddRefs(doc));
    NS_ENSURE_TRUE(doc, NS_ERROR_UNEXPECTED);

    nsCOMPtr<nsIPresShell> presShell;
    doc->GetShellAt(0, getter_AddRefs(presShell));
    NS_ENSURE_TRUE(presShell, NS_ERROR_UNEXPECTED);

    nsCOMPtr<nsIStyleContext> sContext;
    nsresult rv = GetStyleContextForContent(aContent, presShell,
                                            getter_AddRefs(sContext));
    NS_ENSURE_SUCCESS(rv, rv);

    return sContext->GetRuleNode(aRuleNode);
}
