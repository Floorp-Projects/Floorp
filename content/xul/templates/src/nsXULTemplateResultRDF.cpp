/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Initial Developer of the Original Code is Neil Deakin
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsXULTemplateResultRDF.h"
#include "nsXULContentUtils.h"

// XXXndeakin for some reason, making this class have classinfo breaks trees.
//#include "nsIDOMClassInfo.h"

NS_IMPL_CYCLE_COLLECTION_1(nsXULTemplateResultRDF, mQuery)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsXULTemplateResultRDF)
  NS_INTERFACE_MAP_ENTRY(nsIXULTemplateResult)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsXULTemplateResultRDF)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsXULTemplateResultRDF)

nsXULTemplateResultRDF::nsXULTemplateResultRDF(nsIRDFResource* aNode)
    : mQuery(nsnull),
      mNode(aNode)
{
}

nsXULTemplateResultRDF::nsXULTemplateResultRDF(nsRDFQuery* aQuery,
                                               const Instantiation& aInst,
                                               nsIRDFResource *aNode)
    : mQuery(aQuery),
      mNode(aNode),
      mInst(aInst)
{
}

nsXULTemplateResultRDF::~nsXULTemplateResultRDF()
{
}

NS_IMETHODIMP
nsXULTemplateResultRDF::GetIsContainer(bool* aIsContainer)
{
    *aIsContainer = PR_FALSE;

    if (mNode) {
        nsXULTemplateQueryProcessorRDF* processor = GetProcessor();
        if (processor)
            return processor->CheckContainer(mNode, aIsContainer);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateResultRDF::GetIsEmpty(bool* aIsEmpty)
{
    *aIsEmpty = PR_TRUE;

    if (mNode) {
        nsXULTemplateQueryProcessorRDF* processor = GetProcessor();
        if (processor)
            return processor->CheckEmpty(mNode, aIsEmpty);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateResultRDF::GetMayProcessChildren(bool* aMayProcessChildren)
{
    // RDF always allows recursion
    *aMayProcessChildren = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateResultRDF::GetId(nsAString& aId)
{
    if (! mNode)
        return NS_ERROR_FAILURE;

    const char* uri;
    mNode->GetValueConst(&uri);

    CopyUTF8toUTF16(uri, aId);

    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateResultRDF::GetResource(nsIRDFResource** aResource)
{
    *aResource = mNode;
    NS_IF_ADDREF(*aResource);
    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateResultRDF::GetType(nsAString& aType)
{
    aType.Truncate();

    nsresult rv = NS_OK;

    nsXULTemplateQueryProcessorRDF* processor = GetProcessor();
    if (processor) {
        bool found;
        rv = processor->CheckIsSeparator(mNode, &found);
        if (NS_SUCCEEDED(rv) && found)
            aType.AssignLiteral("separator");
    }

    return rv;
}

NS_IMETHODIMP
nsXULTemplateResultRDF::GetBindingFor(nsIAtom* aVar, nsAString& aValue)
{
    nsCOMPtr<nsIRDFNode> val;
    GetAssignment(aVar, getter_AddRefs(val));

    return nsXULContentUtils::GetTextForNode(val, aValue);
}

NS_IMETHODIMP
nsXULTemplateResultRDF::GetBindingObjectFor(nsIAtom* aVar, nsISupports** aValue)
{
    GetAssignment(aVar, (nsIRDFNode **)aValue);

    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateResultRDF::RuleMatched(nsISupports* aQuery, nsIDOMNode* aRuleNode)
{
    // when a rule matches, set the bindings that must be used.
    nsXULTemplateQueryProcessorRDF* processor = GetProcessor();
    if (processor) {
        RDFBindingSet* bindings = processor->GetBindingsForRule(aRuleNode);
        if (bindings) {
            nsresult rv = mBindingValues.SetBindingSet(bindings);
            if (NS_FAILED(rv))
                return rv;

            bindings->AddDependencies(mNode, this);
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateResultRDF::HasBeenRemoved()
{
    // when a result is no longer used, clean up the dependencies and
    // memory elements that refer to it
    mBindingValues.RemoveDependencies(mNode, this);

    nsXULTemplateQueryProcessorRDF* processor = GetProcessor();
    if (processor)
        processor->RemoveMemoryElements(mInst, this);

    return NS_OK;
}


void
nsXULTemplateResultRDF::GetAssignment(nsIAtom* aVar, nsIRDFNode** aValue)
{
    // look up a variable in the assignments map
    *aValue = nsnull;
    mInst.mAssignments.GetAssignmentFor(aVar, aValue);

    // if not found, look up the variable in the bindings
    if (! *aValue)
        mBindingValues.GetAssignmentFor(this, aVar, aValue);
}


bool
nsXULTemplateResultRDF::SyncAssignments(nsIRDFResource* aSubject,
                                        nsIRDFResource* aPredicate,
                                        nsIRDFNode* aTarget)
{
    // synchronize the bindings when an assertion is added or removed
    RDFBindingSet* bindingset = mBindingValues.GetBindingSet();
    if (bindingset) {
        return bindingset->SyncAssignments(aSubject, aPredicate, aTarget,
            (aSubject == mNode) ? mQuery->GetMemberVariable() : nsnull,
            this, mBindingValues);
    }

    return PR_FALSE;
}

bool
nsXULTemplateResultRDF::HasMemoryElement(const MemoryElement& aMemoryElement)
{
    MemoryElementSet::ConstIterator last = mInst.mSupport.Last();
    for (MemoryElementSet::ConstIterator element = mInst.mSupport.First();
                                         element != last; ++element) {
        if ((*element).Equals(aMemoryElement))
            return PR_TRUE;
    }

    return PR_FALSE;
}
