/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Waterson <waterson@netscape.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsConflictSet.h"
#include "nsContentTestNode.h"
#include "nsISupportsArray.h"
#include "nsIXULDocument.h"
#include "nsIRDFResource.h"
#include "nsIAtom.h"
#include "nsXULContentUtils.h"

#include "prlog.h"
#ifdef PR_LOGGING
extern PRLogModuleInfo* gXULTemplateLog;
#endif

PRBool
IsElementContainedBy(nsIContent* aElement, nsIContent* aContainer)
{
    // Make sure that we're actually creating content for the tree
    // content model that we've been assigned to deal with.

    // Walk up the parent chain from us to the root and
    // see what we find.
    if (aElement == aContainer)
        return PR_TRUE;

    // walk up the tree until you find rootAtom
    nsCOMPtr<nsIContent> element(do_QueryInterface(aElement));
    nsCOMPtr<nsIContent> parent;
    element->GetParent(*getter_AddRefs(parent));
    element = parent;
    
    while (element) {
        if (element.get() == aContainer)
            return PR_TRUE;

        element->GetParent(*getter_AddRefs(parent));
        element = parent;
    }
    
    return PR_FALSE;
}

nsContentTestNode::nsContentTestNode(InnerNode* aParent,
                                     nsConflictSet& aConflictSet,
                                     nsIXULDocument* aDocument,
                                     nsIContent* aRoot,
                                     PRInt32 aContentVariable,
                                     PRInt32 aIdVariable,
                                     nsIAtom* aTag)
    : TestNode(aParent),
      mConflictSet(aConflictSet),
      mDocument(aDocument),
      mRoot(aRoot),
      mContentVariable(aContentVariable),
      mIdVariable(aIdVariable),
      mTag(aTag)
{
#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
        nsAutoString tag(NS_LITERAL_STRING("(none)"));
        if (mTag)
            mTag->ToString(tag);

        PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
               ("nsContentTestNode[%p]: parent=%p content-var=%d id-var=%d tag=%s",
                this, aParent, mContentVariable, mIdVariable,
                NS_ConvertUCS2toUTF8(tag).get()));
    }
#endif
}

nsresult
nsContentTestNode::FilterInstantiations(InstantiationSet& aInstantiations, void* aClosure) const
{
    nsresult rv;

    nsCOMPtr<nsISupportsArray> elements;
    rv = NS_NewISupportsArray(getter_AddRefs(elements));
    if (NS_FAILED(rv)) return rv;

    InstantiationSet::Iterator last = aInstantiations.Last();
    for (InstantiationSet::Iterator inst = aInstantiations.First(); inst != last; ++inst) {
        Value contentValue;
        PRBool hasContentBinding = inst->mAssignments.GetAssignmentFor(mContentVariable, &contentValue);

        Value idValue;
        PRBool hasIdBinding = inst->mAssignments.GetAssignmentFor(mIdVariable, &idValue);

        if (hasContentBinding && hasIdBinding) {
            // both are bound, consistency check
            PRBool consistent = PR_TRUE;

            nsIContent* content = VALUE_TO_ICONTENT(contentValue);

            if (mTag) {
                // If we're supposed to be checking the tag, do it now.
                nsCOMPtr<nsIAtom> tag;
                content->GetTag(*getter_AddRefs(tag));

                if (tag != mTag)
                    consistent = PR_FALSE;
            }

            if (consistent) {
                nsCOMPtr<nsIRDFResource> resource;
                nsXULContentUtils::GetElementRefResource(content, getter_AddRefs(resource));
            
                if (resource.get() != VALUE_TO_IRDFRESOURCE(idValue))
                    consistent = PR_FALSE;
            }

            if (consistent) {
                Element* element =
                    nsContentTestNode::Element::Create(mConflictSet.GetPool(),
                                    VALUE_TO_ICONTENT(contentValue));

                if (! element)
                    return NS_ERROR_OUT_OF_MEMORY;

                inst->AddSupportingElement(element);
            }
            else {
                aInstantiations.Erase(inst--);
            }
        }
        else if (hasContentBinding) {
            // the content node is bound, get its id
            PRBool consistent = PR_TRUE;

            nsIContent* content = VALUE_TO_ICONTENT(contentValue);

            if (mTag) {
                // If we're supposed to be checking the tag, do it now.
                nsCOMPtr<nsIAtom> tag;
                content->GetTag(*getter_AddRefs(tag));

                if (tag != mTag)
                    consistent = PR_FALSE;
            }

            if (consistent) {
                nsCOMPtr<nsIRDFResource> resource;
                nsXULContentUtils::GetElementRefResource(content, getter_AddRefs(resource));

                if (resource) {
                    Instantiation newinst = *inst;
                    newinst.AddAssignment(mIdVariable, Value(resource.get()));

                    Element* element =
                        nsContentTestNode::Element::Create(mConflictSet.GetPool(), content);

                    if (! element)
                        return NS_ERROR_OUT_OF_MEMORY;

                    newinst.AddSupportingElement(element);

                    aInstantiations.Insert(inst, newinst);
                }
            }

            aInstantiations.Erase(inst--);
        }
        else if (hasIdBinding) {
            // the 'id' is bound, find elements in the content tree that match
            const char* uri;
            rv = VALUE_TO_IRDFRESOURCE(idValue)->GetValueConst(&uri);
            if (NS_FAILED(rv)) return rv;

            rv = mDocument->GetElementsForID(NS_ConvertUTF8toUCS2(uri), elements);
            if (NS_FAILED(rv)) return rv;
            PRUint32 count;
            rv = elements->Count(&count);
            if (NS_FAILED(rv)) return rv;

            for (PRInt32 j = PRInt32(count) - 1; j >= 0; --j) {
                nsISupports* isupports = elements->ElementAt(j);
                nsCOMPtr<nsIContent> content = do_QueryInterface(isupports);
                NS_IF_RELEASE(isupports);

                if (IsElementContainedBy(content, mRoot)) {
                    if (mTag) {
                        // If we've got a tag, check it to ensure
                        // we're consistent.
                        nsCOMPtr<nsIAtom> tag;
                        content->GetTag(*getter_AddRefs(tag));

                        if (tag != mTag)
                            continue;
                    }

                    Instantiation newinst = *inst;
                    newinst.AddAssignment(mContentVariable, Value(content.get()));

                    Element* element =
                        nsContentTestNode::Element::Create(mConflictSet.GetPool(), content);

                    if (! element)
                        return NS_ERROR_OUT_OF_MEMORY;

                    newinst.AddSupportingElement(element);

                    aInstantiations.Insert(inst, newinst);
                }
            }

            aInstantiations.Erase(inst--);
        }
    }

    return NS_OK;
}

nsresult
nsContentTestNode::GetAncestorVariables(VariableSet& aVariables) const
{
    aVariables.Add(mContentVariable);
    aVariables.Add(mIdVariable);

    return TestNode::GetAncestorVariables(aVariables);
}

