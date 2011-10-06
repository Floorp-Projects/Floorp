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
 * Portions created by the Initial Developer are Copyright (C) 2006
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

#include "nsIServiceManager.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIContent.h"

#include "nsIRDFService.h"

#include "nsXULTemplateResultXML.h"
#include "nsXMLBinding.h"

static PRUint32 sTemplateId = 0;

NS_IMPL_ISUPPORTS1(nsXULTemplateResultXML, nsIXULTemplateResult)

nsXULTemplateResultXML::nsXULTemplateResultXML(nsXMLQuery* aQuery,
                                               nsIDOMNode* aNode,
                                               nsXMLBindingSet* aBindings)
    : mQuery(aQuery), mNode(aNode)
{
    nsCOMPtr<nsIContent> content = do_QueryInterface(mNode);

    // If the node has an id, create the uri from it. Otherwise, there isn't
    // anything to identify the node with so just use a somewhat random number.
    nsCOMPtr<nsIAtom> id = content->GetID();
    if (id) {
      nsCOMPtr<nsIURI> uri = content->GetBaseURI();
      nsCAutoString spec;
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
    if (mNode)
        mNode->HasChildNodes(aIsContainer);
    else
        *aIsContainer = PR_FALSE;
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
                *aIsEmpty = PR_FALSE;
                return NS_OK;
            }
        }
    }

    *aIsEmpty = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateResultXML::GetMayProcessChildren(bool* aMayProcessChildren)
{
    *aMayProcessChildren = PR_TRUE;
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
    *aResource = nsnull;
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

    PRInt32 idx = mRequiredValues.LookupTargetIndex(aVar, &binding);
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
    nsCOMPtr<nsIDOMNode> node;

    if (mQuery && aVar == mQuery->GetMemberVariable()) {
        node = mNode;
    }
    else {
        PRInt32 idx = mRequiredValues.LookupTargetIndex(aVar, &binding);
        if (idx > 0) {
            mRequiredValues.GetNodeAssignmentFor(this, binding, idx,
                                                 getter_AddRefs(node));
        }
        else {
            idx = mOptionalValues.LookupTargetIndex(aVar, &binding);
            if (idx > 0) {
                mOptionalValues.GetNodeAssignmentFor(this, binding, idx,
                                                     getter_AddRefs(node));
            }
        }
    }

    *aValue = node;
    NS_IF_ADDREF(*aValue);
    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateResultXML::RuleMatched(nsISupports* aQueryNode,
                                    nsIDOMNode* aRuleNode)
{
    // when a rule matches, set the bindings that must be used.
    nsXULTemplateQueryProcessorXML* processor = mQuery ? mQuery->Processor() :
                                                         nsnull;
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

void
nsXULTemplateResultXML::GetNode(nsIDOMNode** aNode)
{
    *aNode = mNode;
    NS_IF_ADDREF(*aNode);
}
