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

#include "nsRDFConMemberTestNode.h"
#include "nsIRDFContainer.h"
#include "nsIRDFContainerUtils.h"
#include "nsRDFCID.h"
#include "nsIServiceManager.h"
#include "nsResourceSet.h"
#include "nsConflictSet.h"
#include "nsString.h"

#include "prlog.h"
#ifdef PR_LOGGING
#include "nsXULContentUtils.h"
extern PRLogModuleInfo* gXULTemplateLog;
#endif

nsRDFConMemberTestNode::nsRDFConMemberTestNode(InnerNode* aParent,
                                               nsConflictSet& aConflictSet,
                                               nsIRDFDataSource* aDataSource,
                                               const nsResourceSet& aMembershipProperties,
                                               PRInt32 aContainerVariable,
                                               PRInt32 aMemberVariable)
    : nsRDFTestNode(aParent),
      mConflictSet(aConflictSet),
      mDataSource(aDataSource),
      mMembershipProperties(aMembershipProperties),
      mContainerVariable(aContainerVariable),
      mMemberVariable(aMemberVariable)
{
#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
        nsCAutoString props;

        nsResourceSet::ConstIterator last = aMembershipProperties.Last();
        nsResourceSet::ConstIterator first = aMembershipProperties.First();
        nsResourceSet::ConstIterator iter;

        for (iter = first; iter != last; ++iter) {
            if (iter != first)
                props += " ";

            const char* str;
            iter->GetValueConst(&str);

            props += str;
        }

        PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
               ("nsRDFConMemberTestNode[%p]: parent=%p member-props=(%s) container-var=%d member-var=%d",
                this,
                aParent,
                props.get(),
                mContainerVariable,
                mMemberVariable));
    }
#endif
}

nsresult
nsRDFConMemberTestNode::FilterInstantiations(InstantiationSet& aInstantiations, void* aClosure) const
{
    // XXX Uh, factor me, please!
    nsresult rv;

    nsCOMPtr<nsIRDFContainerUtils> rdfc =
        do_GetService("@mozilla.org/rdf/container-utils;1");

    if (! rdfc)
        return NS_ERROR_FAILURE;

    InstantiationSet::Iterator last = aInstantiations.Last();
    for (InstantiationSet::Iterator inst = aInstantiations.First(); inst != last; ++inst) {
        PRBool hasContainerBinding;
        Value containerValue;
        hasContainerBinding = inst->mAssignments.GetAssignmentFor(mContainerVariable, &containerValue);

        nsCOMPtr<nsIRDFContainer> rdfcontainer;

        if (hasContainerBinding) {
            // If we have a container assignment, then see if the
            // container is an RDF container (bag, seq, alt), and if
            // so, wrap it.
            PRBool isRDFContainer;
            rv = rdfc->IsContainer(mDataSource,
                                   VALUE_TO_IRDFRESOURCE(containerValue),
                                   &isRDFContainer);
            if (NS_FAILED(rv)) return rv;

            if (isRDFContainer) {
                rdfcontainer = do_CreateInstance("@mozilla.org/rdf/container;1", &rv);
                if (NS_FAILED(rv)) return rv;

                rv = rdfcontainer->Init(mDataSource, VALUE_TO_IRDFRESOURCE(containerValue));
                if (NS_FAILED(rv)) return rv;
            }
        }

        PRBool hasMemberBinding;
        Value memberValue;
        hasMemberBinding = inst->mAssignments.GetAssignmentFor(mMemberVariable, &memberValue);

#ifdef PR_LOGGING
        if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
            const char* container = "(unbound)";
            if (hasContainerBinding)
                VALUE_TO_IRDFRESOURCE(containerValue)->GetValueConst(&container);

            nsAutoString member(NS_LITERAL_STRING("(unbound)"));
            if (hasMemberBinding)
                nsXULContentUtils::GetTextForNode(VALUE_TO_IRDFRESOURCE(memberValue), member);

            PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
                   ("nsRDFConMemberTestNode[%p]: FilterInstantiations() container=[%s] member=[%s]",
                    this, container, NS_ConvertUCS2toUTF8(member).get()));
        }
#endif

        if (hasContainerBinding && hasMemberBinding) {
            // it's a consistency check. see if we have a assignment that is consistent
            PRBool isconsistent = PR_FALSE;

            if (rdfcontainer) {
                // RDF containers are easy. Just use the container API.
                PRInt32 index;
                rv = rdfcontainer->IndexOf(VALUE_TO_IRDFRESOURCE(memberValue), &index);
                if (NS_FAILED(rv)) return rv;

                if (index >= 0)
                    isconsistent = PR_TRUE;
            }

            // XXXwaterson oof. if we *are* an RDF container, why do
            // we still need to grovel through all the containment
            // properties if the thing we're looking for wasn't there?

            if (! isconsistent) {
                // Othewise, we'll need to grovel through the
                // membership properties to see if we have an
                // assertion that indicates membership.
                for (nsResourceSet::ConstIterator property = mMembershipProperties.First();
                     property != mMembershipProperties.Last();
                     ++property) {
                    PRBool hasAssertion;
                    rv = mDataSource->HasAssertion(VALUE_TO_IRDFRESOURCE(containerValue),
                                                   *property,
                                                   VALUE_TO_IRDFNODE(memberValue),
                                                   PR_TRUE,
                                                   &hasAssertion);
                    if (NS_FAILED(rv)) return rv;

                    if (hasAssertion) {
                        // it's consistent. leave it in the set and we'll
                        // run it up to our parent.
                        isconsistent = PR_TRUE;
                        break;
                    }
                }
            }

            PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
                   ("    consistency check => %s", isconsistent ? "passed" : "failed"));

            if (isconsistent) {
                // Add a memory element to our set-of-support.
                Element* element =
                    nsRDFConMemberTestNode::Element::Create(mConflictSet.GetPool(),
                                                            VALUE_TO_IRDFRESOURCE(containerValue),
                                                            VALUE_TO_IRDFNODE(memberValue));

                if (! element)
                    return NS_ERROR_OUT_OF_MEMORY;

                inst->AddSupportingElement(element);
            }
            else {
                // it's inconsistent. remove it.
                aInstantiations.Erase(inst--);
            }

            // We're done, go on to the next instantiation
            continue;
        }

        if (hasContainerBinding && rdfcontainer) {
            // We've got a container assignment, and the container is
            // bound to an RDF container. Add each member as a new
            // instantiation.
            nsCOMPtr<nsISimpleEnumerator> elements;
            rv = rdfcontainer->GetElements(getter_AddRefs(elements));
            if (NS_FAILED(rv)) return rv;

            while (1) {
                PRBool hasmore;
                rv = elements->HasMoreElements(&hasmore);
                if (NS_FAILED(rv)) return rv;

                if (! hasmore)
                    break;

                nsCOMPtr<nsISupports> isupports;
                rv = elements->GetNext(getter_AddRefs(isupports));
                if (NS_FAILED(rv)) return rv;

                nsCOMPtr<nsIRDFNode> node = do_QueryInterface(isupports);
                if (! node)
                    return NS_ERROR_UNEXPECTED;

#ifdef PR_LOGGING
                if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
                    nsAutoString member;
                    nsXULContentUtils::GetTextForNode(node, member);

                    PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
                           ("    member => %s", NS_ConvertUCS2toUTF8(member).get()));
                }
#endif

                Instantiation newinst = *inst;
                newinst.AddAssignment(mMemberVariable, Value(node.get()));

                Element* element =
                    nsRDFConMemberTestNode::Element::Create(mConflictSet.GetPool(),
                                                            VALUE_TO_IRDFRESOURCE(containerValue),
                                                            node);

                if (! element)
                    return NS_ERROR_OUT_OF_MEMORY;

                newinst.AddSupportingElement(element);

                aInstantiations.Insert(inst, newinst);
            }
        }

        if (hasMemberBinding) {
            // Oh, this is so nasty. If we have a member assignment, then
            // grovel through each one of our inbound arcs to see if
            // any of them are ordinal properties (like an RDF
            // container might have). If so, walk it backwards to get
            // the container we're in.
            nsCOMPtr<nsISimpleEnumerator> arcsin;
            rv = mDataSource->ArcLabelsIn(VALUE_TO_IRDFNODE(memberValue), getter_AddRefs(arcsin));
            if (NS_FAILED(rv)) return rv;

            while (1) {
                nsCOMPtr<nsIRDFResource> property;

                {
                    PRBool hasmore;
                    rv = arcsin->HasMoreElements(&hasmore);
                    if (NS_FAILED(rv)) return rv;

                    if (! hasmore)
                        break;

                    nsCOMPtr<nsISupports> isupports;
                    rv = arcsin->GetNext(getter_AddRefs(isupports));
                    if (NS_FAILED(rv)) return rv;

                    property = do_QueryInterface(isupports);
                    if (! property)
                        return NS_ERROR_UNEXPECTED;
                }

                // Ordinal properties automagically indicate container
                // membership as far as we're concerned. Note that
                // we're *only* concerned with ordinal properties
                // here: the next block will worry about the other
                // membership properties.
                PRBool isordinal;
                rv = rdfc->IsOrdinalProperty(property, &isordinal);
                if (NS_FAILED(rv)) return rv;

                if (isordinal) {
                    // If we get here, we've found a property that
                    // indicates container membership leading *into* a
                    // member node. Find all the people that point to
                    // it, and call them containers.
                    nsCOMPtr<nsISimpleEnumerator> sources;
                    rv = mDataSource->GetSources(property, VALUE_TO_IRDFNODE(memberValue), PR_TRUE,
                                                 getter_AddRefs(sources));
                    if (NS_FAILED(rv)) return rv;

                    while (1) {
                        PRBool hasmore;
                        rv = sources->HasMoreElements(&hasmore);
                        if (NS_FAILED(rv)) return rv;

                        if (! hasmore)
                            break;

                        nsCOMPtr<nsISupports> isupports;
                        rv = sources->GetNext(getter_AddRefs(isupports));
                        if (NS_FAILED(rv)) return rv;

                        nsCOMPtr<nsIRDFResource> source = do_QueryInterface(isupports);
                        if (! source)
                            return NS_ERROR_UNEXPECTED;

#ifdef PR_LOGGING
                        if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
                            const char* container;
                            source->GetValueConst(&container);

                            PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
                                   ("    container => %s", container));
                        }
#endif

                        // Add a new instantiation
                        Instantiation newinst = *inst;
                        newinst.AddAssignment(mContainerVariable, Value(source.get()));

                        Element* element =
                            nsRDFConMemberTestNode::Element::Create(mConflictSet.GetPool(),
                                                                    source,
                                                                    VALUE_TO_IRDFNODE(memberValue));

                        if (! element)
                            return NS_ERROR_OUT_OF_MEMORY;

                        newinst.AddSupportingElement(element);

                        aInstantiations.Insert(inst, newinst);
                    }
                }
            }
        }

        if ((hasContainerBinding && ! hasMemberBinding) ||
            (! hasContainerBinding && hasMemberBinding)) {
            // it's an open ended query on the container or member. go
            // through our containment properties to see if anything
            // applies.
            for (nsResourceSet::ConstIterator property = mMembershipProperties.First();
                 property != mMembershipProperties.Last();
                 ++property) {
                nsCOMPtr<nsISimpleEnumerator> results;
                if (hasContainerBinding) {
                    rv = mDataSource->GetTargets(VALUE_TO_IRDFRESOURCE(containerValue), *property, PR_TRUE,
                                                 getter_AddRefs(results));
                }
                else {
                    rv = mDataSource->GetSources(*property, VALUE_TO_IRDFNODE(memberValue), PR_TRUE,
                                                 getter_AddRefs(results));
                }
                if (NS_FAILED(rv)) return rv;

                while (1) {
                    PRBool hasmore;
                    rv = results->HasMoreElements(&hasmore);
                    if (NS_FAILED(rv)) return rv;

                    if (! hasmore)
                        break;

                    nsCOMPtr<nsISupports> isupports;
                    rv = results->GetNext(getter_AddRefs(isupports));
                    if (NS_FAILED(rv)) return rv;

                    PRInt32 variable;
                    Value value;

                    if (hasContainerBinding) {
                        variable = mMemberVariable;

                        nsCOMPtr<nsIRDFNode> member = do_QueryInterface(isupports);
                        NS_ASSERTION(member != nsnull, "member is not an nsIRDFNode");
                        if (! member) continue;

#ifdef PR_LOGGING
                        if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
                            nsAutoString s;
                            nsXULContentUtils::GetTextForNode(member, s);

                            PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
                                   ("    member => %s", NS_ConvertUCS2toUTF8(s).get()));
                        }
#endif

                        value = member.get();
                    }
                    else {
                        variable = mContainerVariable;

                        nsCOMPtr<nsIRDFResource> container = do_QueryInterface(isupports);
                        NS_ASSERTION(container != nsnull, "container is not an nsIRDFResource");
                        if (! container) continue;

#ifdef PR_LOGGING
                        if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
                            const char* s;
                            container->GetValueConst(&s);

                            PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
                                   ("    container => %s", s));
                        }
#endif

                        value = container.get();
                    }

                    // Copy the original instantiation, and add it to the
                    // instantiation set with the new assignment that we've
                    // introduced. Ownership will be transferred to the
                    Instantiation newinst = *inst;
                    newinst.AddAssignment(variable, value);

                    Element* element;
                    if (hasContainerBinding) {
                        element =
                            nsRDFConMemberTestNode::Element::Create(mConflictSet.GetPool(),
                                                                    VALUE_TO_IRDFRESOURCE(containerValue),
                                                                    VALUE_TO_IRDFNODE(value));
                    }
                    else {
                        element =
                            nsRDFConMemberTestNode::Element::Create(mConflictSet.GetPool(),
                                                                    VALUE_TO_IRDFRESOURCE(value),
                                                                    VALUE_TO_IRDFNODE(memberValue));
                    }

                    if (! element)
                        return NS_ERROR_OUT_OF_MEMORY;

                    newinst.AddSupportingElement(element);

                    aInstantiations.Insert(inst, newinst);
                }
            }
        }

        if (! hasContainerBinding && ! hasMemberBinding) {
            // Neither container nor member assignment!
            NS_ERROR("can't do open ended queries like that!");
            return NS_ERROR_UNEXPECTED;
        }

        // finally, remove the "under specified" instantiation.
        aInstantiations.Erase(inst--);
    }

    return NS_OK;
}

nsresult
nsRDFConMemberTestNode::GetAncestorVariables(VariableSet& aVariables) const
{
    nsresult rv;

    rv = aVariables.Add(mContainerVariable);
    if (NS_FAILED(rv)) return rv;

    rv = aVariables.Add(mMemberVariable);
    if (NS_FAILED(rv)) return rv;

    return TestNode::GetAncestorVariables(aVariables);
}


PRBool
nsRDFConMemberTestNode::CanPropogate(nsIRDFResource* aSource,
                                     nsIRDFResource* aProperty,
                                     nsIRDFNode* aTarget,
                                     Instantiation& aInitialBindings) const
{
    nsresult rv;

    PRBool canpropogate = PR_FALSE;

    nsCOMPtr<nsIRDFContainerUtils> rdfc =
        do_GetService("@mozilla.org/rdf/container-utils;1");

    if (! rdfc)
        return NS_ERROR_FAILURE;

    // We can certainly propogate ordinal properties
    rv = rdfc->IsOrdinalProperty(aProperty, &canpropogate);
    if (NS_FAILED(rv)) return PR_FALSE;

    if (! canpropogate) {
        canpropogate = mMembershipProperties.Contains(aProperty);
    }

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
        const char* source;
        aSource->GetValueConst(&source);

        const char* property;
        aProperty->GetValueConst(&property);

        nsAutoString target;
        nsXULContentUtils::GetTextForNode(aTarget, target);

        PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
               ("nsRDFConMemberTestNode[%p]: CanPropogate([%s]==[%s]=>[%s]) => %s",
                this, source, property, NS_ConvertUCS2toUTF8(target).get(),
                canpropogate ? "true" : "false"));
    }
#endif

    if (canpropogate) {
        aInitialBindings.AddAssignment(mContainerVariable, Value(aSource));
        aInitialBindings.AddAssignment(mMemberVariable, Value(aTarget));
        return PR_TRUE;
    }

    return PR_FALSE;
}

void
nsRDFConMemberTestNode::Retract(nsIRDFResource* aSource,
                                nsIRDFResource* aProperty,
                                nsIRDFNode* aTarget,
                                nsTemplateMatchSet& aFirings,
                                nsTemplateMatchSet& aRetractions) const
{
    PRBool canretract = PR_FALSE;

    nsCOMPtr<nsIRDFContainerUtils> rdfc =
        do_GetService("@mozilla.org/rdf/container-utils;1");

    if (! rdfc)
        return;

    // We can certainly retract ordinal properties
    nsresult rv;
    rv = rdfc->IsOrdinalProperty(aProperty, &canretract);
    if (NS_FAILED(rv)) return;

    if (! canretract) {
        canretract = mMembershipProperties.Contains(aProperty);
    }

    if (canretract) {
        mConflictSet.Remove(Element(aSource, aTarget), aFirings, aRetractions);
    }
}
