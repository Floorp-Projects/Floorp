/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIServiceManager.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIContent.h"

#include "nsIRDFService.h"

#include "nsXULTemplateResultXML.h"
#include "nsXMLBinding.h"

static uint32_t sTemplateId = 0;

NS_IMPL_ISUPPORTS(nsXULTemplateResultXML, nsIXULTemplateResult)

nsXULTemplateResultXML::nsXULTemplateResultXML(nsXMLQuery* aQuery,
                                               nsIContent* aNode,
                                               nsXMLBindingSet* aBindings)
    : mQuery(aQuery), mNode(aNode)
{
    // If the node has an id, create the uri from it. Otherwise, there isn't
    // anything to identify the node with so just use a somewhat random number.
    nsCOMPtr<nsIAtom> id = mNode->GetID();
    if (id) {
      nsCOMPtr<nsIURI> uri = mNode->GetBaseURI();
      nsAutoCString spec;
      uri->GetSpec(spec);

      mId = NS_ConvertUTF8toUTF16(spec);

      nsAutoString idstr;
      id->ToString(idstr);
      mId += NS_LITERAL_STRING("#") + idstr;
    }
    else {
      nsAutoString rowid(NS_LITERAL_STRING("row"));
      rowid.AppendInt(++sTemplateId);
      mId.Assign(rowid);
    }

    if (aBindings)
        mRequiredValues.SetBindingSet(aBindings);
}

NS_IMETHODIMP
nsXULTemplateResultXML::GetIsContainer(bool* aIsContainer)
{
    // a node is considered a container if it has children
    *aIsContainer = mNode && mNode->HasChildNodes();
    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateResultXML::GetIsEmpty(bool* aIsEmpty)
{
    // a node is considered empty if it has no elements as children
    nsCOMPtr<nsIContent> content = do_QueryInterface(mNode);
    if (content) {
        for (nsIContent* child = content->GetFirstChild();
             child;
             child = child->GetNextSibling()) {
            if (child->IsElement()) {
                *aIsEmpty = false;
                return NS_OK;
            }
        }
    }

    *aIsEmpty = true;
    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateResultXML::GetMayProcessChildren(bool* aMayProcessChildren)
{
    *aMayProcessChildren = true;
    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateResultXML::GetId(nsAString& aId)
{
    aId = mId;
    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateResultXML::GetResource(nsIRDFResource** aResource)
{
    *aResource = nullptr;
    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateResultXML::GetType(nsAString& aType)
{
    aType.Truncate();
    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateResultXML::GetBindingFor(nsIAtom* aVar, nsAString& aValue)
{
    NS_ENSURE_ARG_POINTER(aVar);

    // get the position of the atom in the variables table
    nsXMLBinding* binding;

    int32_t idx = mRequiredValues.LookupTargetIndex(aVar, &binding);
    if (idx >= 0) {
        mRequiredValues.GetStringAssignmentFor(this, binding, idx, aValue);
        return NS_OK;
    }

    idx = mOptionalValues.LookupTargetIndex(aVar, &binding);
    if (idx >= 0) {
        mOptionalValues.GetStringAssignmentFor(this, binding, idx, aValue);
        return NS_OK;
    }

    // if the variable is not bound, just use the variable name as the name of
    // an attribute to retrieve
    nsAutoString attr;
    aVar->ToString(attr);

    if (attr.Length() > 1) {
        nsCOMPtr<nsIDOMElement> element = do_QueryInterface(mNode);
        if (element)
            return element->GetAttribute(Substring(attr, 1), aValue);
    }

    aValue.Truncate();
    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateResultXML::GetBindingObjectFor(nsIAtom* aVar, nsISupports** aValue)
{
    NS_ENSURE_ARG_POINTER(aVar);

    nsXMLBinding* binding;
    nsCOMPtr<nsISupports> node;

    if (mQuery && aVar == mQuery->GetMemberVariable()) {
        node = mNode;
    }
    else {
        int32_t idx = mRequiredValues.LookupTargetIndex(aVar, &binding);
        if (idx > 0) {
            node = mRequiredValues.GetNodeAssignmentFor(this, binding, idx);
        }
        else {
            idx = mOptionalValues.LookupTargetIndex(aVar, &binding);
            if (idx > 0) {
                node = mOptionalValues.GetNodeAssignmentFor(this, binding, idx);
            }
        }
    }

    node.forget(aValue);
    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateResultXML::RuleMatched(nsISupports* aQueryNode,
                                    nsIDOMNode* aRuleNode)
{
    // when a rule matches, set the bindings that must be used.
    nsXULTemplateQueryProcessorXML* processor = mQuery ? mQuery->Processor() :
                                                         nullptr;
    if (processor) {
        nsXMLBindingSet* bindings =
            processor->GetOptionalBindingsForRule(aRuleNode);
        if (bindings)
            mOptionalValues.SetBindingSet(bindings);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateResultXML::HasBeenRemoved()
{
    return NS_OK;
}
