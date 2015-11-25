/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRDFConMemberTestNode.h"
#include "nsIRDFContainer.h"
#include "nsIRDFContainerUtils.h"
#include "nsRDFCID.h"
#include "nsIServiceManager.h"
#include "nsResourceSet.h"
#include "nsString.h"
#include "nsXULContentUtils.h"

#include "mozilla/Logging.h"

using mozilla::LogLevel;

extern mozilla::LazyLogModule gXULTemplateLog;

nsRDFConMemberTestNode::nsRDFConMemberTestNode(TestNode* aParent,
                                               nsXULTemplateQueryProcessorRDF* aProcessor,
                                               nsIAtom *aContainerVariable,
                                               nsIAtom *aMemberVariable)
    : nsRDFTestNode(aParent),
      mProcessor(aProcessor),
      mContainerVariable(aContainerVariable),
      mMemberVariable(aMemberVariable)
{
    if (MOZ_LOG_TEST(gXULTemplateLog, LogLevel::Debug)) {
        nsAutoCString props;

        nsResourceSet& containmentProps = aProcessor->ContainmentProperties();
        nsResourceSet::ConstIterator last = containmentProps.Last();
        nsResourceSet::ConstIterator first = containmentProps.First();
        nsResourceSet::ConstIterator iter;

        for (iter = first; iter != last; ++iter) {
            if (iter != first)
                props += " ";

            const char* str;
            iter->GetValueConst(&str);

            props += str;
        }

        nsAutoString cvar(NS_LITERAL_STRING("(none)"));
        if (mContainerVariable)
            mContainerVariable->ToString(cvar);

        nsAutoString mvar(NS_LITERAL_STRING("(none)"));
        if (mMemberVariable)
            mMemberVariable->ToString(mvar);

        MOZ_LOG(gXULTemplateLog, LogLevel::Debug,
               ("nsRDFConMemberTestNode[%p]: parent=%p member-props=(%s) container-var=%s member-var=%s",
                this,
                aParent,
                props.get(),
                NS_ConvertUTF16toUTF8(cvar).get(),
                NS_ConvertUTF16toUTF8(mvar).get()));
    }
}

nsresult
nsRDFConMemberTestNode::FilterInstantiations(InstantiationSet& aInstantiations,
                                             bool* aCantHandleYet) const
{
    // XXX Uh, factor me, please!
    nsresult rv;

    if (aCantHandleYet)
        *aCantHandleYet = false;

    nsCOMPtr<nsIRDFContainerUtils> rdfc =
        do_GetService("@mozilla.org/rdf/container-utils;1");

    if (! rdfc)
        return NS_ERROR_FAILURE;

    nsIRDFDataSource* ds = mProcessor->GetDataSource();

    InstantiationSet::Iterator last = aInstantiations.Last();
    for (InstantiationSet::Iterator inst = aInstantiations.First(); inst != last; ++inst) {
        bool hasContainerBinding;
        nsCOMPtr<nsIRDFNode> containerValue;
        hasContainerBinding = inst->mAssignments.GetAssignmentFor(mContainerVariable,
                                                                  getter_AddRefs(containerValue));

        nsCOMPtr<nsIRDFResource> containerRes = do_QueryInterface(containerValue);

        nsCOMPtr<nsIRDFContainer> rdfcontainer;

        if (hasContainerBinding && containerRes) {
            // If we have a container assignment, then see if the
            // container is an RDF container (bag, seq, alt), and if
            // so, wrap it.
            bool isRDFContainer;
            rv = rdfc->IsContainer(ds, containerRes, &isRDFContainer);
            if (NS_FAILED(rv)) return rv;

            if (isRDFContainer) {
                rdfcontainer = do_CreateInstance("@mozilla.org/rdf/container;1", &rv);
                if (NS_FAILED(rv)) return rv;

                rv = rdfcontainer->Init(ds, containerRes);
                if (NS_FAILED(rv)) return rv;
            }
        }

        bool hasMemberBinding;
        nsCOMPtr<nsIRDFNode> memberValue;
        hasMemberBinding = inst->mAssignments.GetAssignmentFor(mMemberVariable,
                                                               getter_AddRefs(memberValue));

        if (MOZ_LOG_TEST(gXULTemplateLog, LogLevel::Debug)) {
            const char* container = "(unbound)";
            if (hasContainerBinding)
                containerRes->GetValueConst(&container);

            nsAutoString member(NS_LITERAL_STRING("(unbound)"));
            if (hasMemberBinding)
                nsXULContentUtils::GetTextForNode(memberValue, member);

            MOZ_LOG(gXULTemplateLog, LogLevel::Debug,
                   ("nsRDFConMemberTestNode[%p]: FilterInstantiations() container=[%s] member=[%s]",
                    this, container, NS_ConvertUTF16toUTF8(member).get()));
        }

        if (hasContainerBinding && hasMemberBinding) {
            // it's a consistency check. see if we have a assignment that is consistent
            bool isconsistent = false;

            if (rdfcontainer) {
                // RDF containers are easy. Just use the container API.
                int32_t index;
                rv = rdfcontainer->IndexOf(memberValue, &index);
                if (NS_FAILED(rv)) return rv;

                if (index >= 0)
                    isconsistent = true;
            }

            // XXXwaterson oof. if we *are* an RDF container, why do
            // we still need to grovel through all the containment
            // properties if the thing we're looking for wasn't there?

            if (! isconsistent) {
                // Othewise, we'll need to grovel through the
                // membership properties to see if we have an
                // assertion that indicates membership.
                nsResourceSet& containmentProps = mProcessor->ContainmentProperties();
                for (nsResourceSet::ConstIterator property = containmentProps.First();
                     property != containmentProps.Last();
                     ++property) {
                    bool hasAssertion;
                    rv = ds->HasAssertion(containerRes,
                                          *property,
                                          memberValue,
                                          true,
                                          &hasAssertion);
                    if (NS_FAILED(rv)) return rv;

                    if (hasAssertion) {
                        // it's consistent. leave it in the set and we'll
                        // run it up to our parent.
                        isconsistent = true;
                        break;
                    }
                }
            }

            MOZ_LOG(gXULTemplateLog, LogLevel::Debug,
                   ("    consistency check => %s", isconsistent ? "passed" : "failed"));

            if (isconsistent) {
                // Add a memory element to our set-of-support.
                Element* element =
                    new nsRDFConMemberTestNode::Element(containerRes,
                                                        memberValue);
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
                bool hasmore;
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

                if (MOZ_LOG_TEST(gXULTemplateLog, LogLevel::Debug)) {
                    nsAutoString member;
                    nsXULContentUtils::GetTextForNode(node, member);

                    MOZ_LOG(gXULTemplateLog, LogLevel::Debug,
                           ("    member => %s", NS_ConvertUTF16toUTF8(member).get()));
                }

                Instantiation newinst = *inst;
                newinst.AddAssignment(mMemberVariable, node);

                Element* element =
                    new nsRDFConMemberTestNode::Element(containerRes, node);
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
            rv = ds->ArcLabelsIn(memberValue, getter_AddRefs(arcsin));
            if (NS_FAILED(rv)) return rv;

            while (1) {
                nsCOMPtr<nsIRDFResource> property;

                {
                    bool hasmore;
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
                bool isordinal;
                rv = rdfc->IsOrdinalProperty(property, &isordinal);
                if (NS_FAILED(rv)) return rv;

                if (isordinal) {
                    // If we get here, we've found a property that
                    // indicates container membership leading *into* a
                    // member node. Find all the people that point to
                    // it, and call them containers.
                    nsCOMPtr<nsISimpleEnumerator> sources;
                    rv = ds->GetSources(property, memberValue, true,
                                        getter_AddRefs(sources));
                    if (NS_FAILED(rv)) return rv;

                    while (1) {
                        bool hasmore;
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

                        if (MOZ_LOG_TEST(gXULTemplateLog, LogLevel::Debug)) {
                            const char* container;
                            source->GetValueConst(&container);

                            MOZ_LOG(gXULTemplateLog, LogLevel::Debug,
                                   ("    container => %s", container));
                        }

                        // Add a new instantiation
                        Instantiation newinst = *inst;
                        newinst.AddAssignment(mContainerVariable, source);

                        Element* element =
                            new nsRDFConMemberTestNode::Element(source,
                                                                memberValue);
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
            nsResourceSet& containmentProps = mProcessor->ContainmentProperties();
            for (nsResourceSet::ConstIterator property = containmentProps.First();
                 property != containmentProps.Last();
                 ++property) {
                nsCOMPtr<nsISimpleEnumerator> results;
                if (hasContainerBinding) {
                    rv = ds->GetTargets(containerRes, *property, true,
                                        getter_AddRefs(results));
                }
                else {
                    rv = ds->GetSources(*property, memberValue, true,
                                        getter_AddRefs(results));
                }
                if (NS_FAILED(rv)) return rv;

                while (1) {
                    bool hasmore;
                    rv = results->HasMoreElements(&hasmore);
                    if (NS_FAILED(rv)) return rv;

                    if (! hasmore)
                        break;

                    nsCOMPtr<nsISupports> isupports;
                    rv = results->GetNext(getter_AddRefs(isupports));
                    if (NS_FAILED(rv)) return rv;

                    nsIAtom* variable;
                    nsCOMPtr<nsIRDFNode> value;
                    nsCOMPtr<nsIRDFResource> valueRes;

                    if (hasContainerBinding) {
                        variable = mMemberVariable;

                        value = do_QueryInterface(isupports);
                        NS_ASSERTION(value != nullptr, "member is not an nsIRDFNode");
                        if (! value) continue;

                        if (MOZ_LOG_TEST(gXULTemplateLog, LogLevel::Debug)) {
                            nsAutoString s;
                            nsXULContentUtils::GetTextForNode(value, s);

                            MOZ_LOG(gXULTemplateLog, LogLevel::Debug,
                                   ("    member => %s", NS_ConvertUTF16toUTF8(s).get()));
                        }
                    }
                    else {
                        variable = mContainerVariable;

                        valueRes = do_QueryInterface(isupports);
                        NS_ASSERTION(valueRes != nullptr, "container is not an nsIRDFResource");
                        if (! valueRes) continue;

                        value = valueRes;

                        if (MOZ_LOG_TEST(gXULTemplateLog, LogLevel::Debug)) {
                            const char* s;
                            valueRes->GetValueConst(&s);

                            MOZ_LOG(gXULTemplateLog, LogLevel::Debug,
                                   ("    container => %s", s));
                        }
                    }

                    // Copy the original instantiation, and add it to the
                    // instantiation set with the new assignment that we've
                    // introduced. Ownership will be transferred to the
                    Instantiation newinst = *inst;
                    newinst.AddAssignment(variable, value);

                    Element* element;
                    if (hasContainerBinding) {
                        element =
                            new nsRDFConMemberTestNode::Element(containerRes, value);
                    }
                    else {
                        element =
                            new nsRDFConMemberTestNode::Element(valueRes, memberValue);
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
            if (!aCantHandleYet) {
                nsXULContentUtils::LogTemplateError(ERROR_TEMPLATE_MEMBER_UNBOUND);
                return NS_ERROR_UNEXPECTED;
            }

            *aCantHandleYet = true;
            return NS_OK;
        }

        // finally, remove the "under specified" instantiation.
        aInstantiations.Erase(inst--);
    }

    return NS_OK;
}

bool
nsRDFConMemberTestNode::CanPropagate(nsIRDFResource* aSource,
                                     nsIRDFResource* aProperty,
                                     nsIRDFNode* aTarget,
                                     Instantiation& aInitialBindings) const
{
    nsresult rv;

    bool canpropagate = false;

    nsCOMPtr<nsIRDFContainerUtils> rdfc =
        do_GetService("@mozilla.org/rdf/container-utils;1");

    if (! rdfc)
        return false;

    // We can certainly propagate ordinal properties
    rv = rdfc->IsOrdinalProperty(aProperty, &canpropagate);
    if (NS_FAILED(rv)) return false;

    if (! canpropagate) {
        canpropagate = mProcessor->ContainmentProperties().Contains(aProperty);
    }

    if (MOZ_LOG_TEST(gXULTemplateLog, LogLevel::Debug)) {
        const char* source;
        aSource->GetValueConst(&source);

        const char* property;
        aProperty->GetValueConst(&property);

        nsAutoString target;
        nsXULContentUtils::GetTextForNode(aTarget, target);

        MOZ_LOG(gXULTemplateLog, LogLevel::Debug,
               ("nsRDFConMemberTestNode[%p]: CanPropagate([%s]==[%s]=>[%s]) => %s",
                this, source, property, NS_ConvertUTF16toUTF8(target).get(),
                canpropagate ? "true" : "false"));
    }

    if (canpropagate) {
        aInitialBindings.AddAssignment(mContainerVariable, aSource);
        aInitialBindings.AddAssignment(mMemberVariable, aTarget);
        return true;
    }

    return false;
}

void
nsRDFConMemberTestNode::Retract(nsIRDFResource* aSource,
                                nsIRDFResource* aProperty,
                                nsIRDFNode* aTarget) const
{
    bool canretract = false;

    nsCOMPtr<nsIRDFContainerUtils> rdfc =
        do_GetService("@mozilla.org/rdf/container-utils;1");

    if (! rdfc)
        return;

    // We can certainly retract ordinal properties
    nsresult rv;
    rv = rdfc->IsOrdinalProperty(aProperty, &canretract);
    if (NS_FAILED(rv)) return;

    if (! canretract) {
        canretract = mProcessor->ContainmentProperties().Contains(aProperty);
    }

    if (canretract) {
        mProcessor->RetractElement(Element(aSource, aTarget));
    }
}
