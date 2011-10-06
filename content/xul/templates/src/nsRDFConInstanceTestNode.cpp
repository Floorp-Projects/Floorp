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

#include "nsIComponentManager.h"
#include "nsIRDFContainer.h"
#include "nsIRDFContainerUtils.h"
#include "nsIServiceManager.h"
#include "nsRDFCID.h"
#include "nsRDFConInstanceTestNode.h"
#include "nsResourceSet.h"

#include "prlog.h"
#ifdef PR_LOGGING
#include "nsXULContentUtils.h"
extern PRLogModuleInfo* gXULTemplateLog;

static const char*
TestToString(nsRDFConInstanceTestNode::Test aTest) {
    switch (aTest) {
    case nsRDFConInstanceTestNode::eFalse:    return "false";
    case nsRDFConInstanceTestNode::eTrue:     return "true";
    case nsRDFConInstanceTestNode::eDontCare: return "dontcare";
    }
    return "?";
}
#endif

nsRDFConInstanceTestNode::nsRDFConInstanceTestNode(TestNode* aParent,
                                                   nsXULTemplateQueryProcessorRDF* aProcessor,
                                                   nsIAtom* aContainerVariable,
                                                   Test aContainer,
                                                   Test aEmpty)
    : nsRDFTestNode(aParent),
      mProcessor(aProcessor),
      mContainerVariable(aContainerVariable),
      mContainer(aContainer),
      mEmpty(aEmpty)
{
#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
        nsCAutoString props;

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

        PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
               ("nsRDFConInstanceTestNode[%p]: parent=%p member-props=(%s) container-var=%s container=%s empty=%s",
                this,
                aParent,
                props.get(),
                NS_ConvertUTF16toUTF8(cvar).get(),
                TestToString(aContainer),
                TestToString(aEmpty)));
    }
#endif
}

nsresult
nsRDFConInstanceTestNode::FilterInstantiations(InstantiationSet& aInstantiations,
                                               bool* aCantHandleYet) const
{
    nsresult rv;

    if (aCantHandleYet)
        *aCantHandleYet = PR_FALSE;

    nsCOMPtr<nsIRDFContainerUtils> rdfc
        = do_GetService("@mozilla.org/rdf/container-utils;1");

    if (! rdfc)
        return NS_ERROR_FAILURE;

    nsIRDFDataSource* ds = mProcessor->GetDataSource();

    InstantiationSet::Iterator last = aInstantiations.Last();
    for (InstantiationSet::Iterator inst = aInstantiations.First(); inst != last; ++inst) {
        nsCOMPtr<nsIRDFNode> value;
        if (! inst->mAssignments.GetAssignmentFor(mContainerVariable, getter_AddRefs(value))) {
            NS_ERROR("can't do unbounded container testing");
            return NS_ERROR_UNEXPECTED;
        }

        nsCOMPtr<nsIRDFResource> valueres = do_QueryInterface(value);
        if (! valueres) {
            aInstantiations.Erase(inst--);
            continue;
        }

#ifdef PR_LOGGING
        if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
            const char* container = "(unbound)";
            valueres->GetValueConst(&container);

            PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
                   ("nsRDFConInstanceTestNode[%p]::FilterInstantiations() container=[%s]",
                    this, container));
        }
#endif

        nsCOMPtr<nsIRDFContainer> rdfcontainer;

        bool isRDFContainer;
        rv = rdfc->IsContainer(ds, valueres, &isRDFContainer);
        if (NS_FAILED(rv)) return rv;

        if (mEmpty != eDontCare || mContainer != eDontCare) {
            Test empty = eDontCare;
            Test container = eDontCare;

            if (isRDFContainer) {
                // It's an RDF container. Use the container utilities
                // to deduce what's in it.
                container = eTrue;

                // XXX should cache the factory
                rdfcontainer = do_CreateInstance("@mozilla.org/rdf/container;1", &rv);
                if (NS_FAILED(rv)) return rv;

                rv = rdfcontainer->Init(ds, valueres);
                if (NS_FAILED(rv)) return rv;

                PRInt32 count;
                rv = rdfcontainer->GetCount(&count);
                if (NS_FAILED(rv)) return rv;

                empty = (count == 0) ? eTrue : eFalse;
            } else {
                empty = eTrue;
                container = eFalse;

                // First do the simple check of finding some outward
                // arcs; there should be only a few containment arcs, so this can
                // save us time from dealing with an iterator later on
                nsResourceSet& containmentProps = mProcessor->ContainmentProperties();
                for (nsResourceSet::ConstIterator property = containmentProps.First();
                     property != containmentProps.Last();
                     ++property) {
                    nsCOMPtr<nsIRDFNode> target;
                    rv = ds->GetTarget(valueres, *property, PR_TRUE, getter_AddRefs(target));
                    if (NS_FAILED(rv)) return rv;

                    if (target != nsnull) {
                        // bingo. we found one.
                        empty = eFalse;
                        container = eTrue;
                        break;
                    }
                }

                // if we still don't think its a container, but we
                // want to know for sure whether it is or not, we need
                // to check ArcLabelsOut for potential container arcs.
                if (container == eFalse && mContainer != eDontCare) {
                    nsCOMPtr<nsISimpleEnumerator> arcsout;
                    rv = ds->ArcLabelsOut(valueres, getter_AddRefs(arcsout));
                    if (NS_FAILED(rv)) return rv;

                    while (1) {
                        bool hasmore;
                        rv = arcsout->HasMoreElements(&hasmore);
                        if (NS_FAILED(rv)) return rv;

                        if (! hasmore)
                            break;

                        nsCOMPtr<nsISupports> isupports;
                        rv = arcsout->GetNext(getter_AddRefs(isupports));
                        if (NS_FAILED(rv)) return rv;

                        nsCOMPtr<nsIRDFResource> property = do_QueryInterface(isupports);
                        NS_ASSERTION(property != nsnull, "not a property");
                        if (! property)
                            return NS_ERROR_UNEXPECTED;

                        if (mProcessor->ContainmentProperties().Contains(property)) {
                            container = eTrue;
                            break;
                        }
                    }
                }
            }

            PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
                   ("    empty => %s",
                    (empty == mEmpty) ? "consistent" : "inconsistent"));

            PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
                   ("    container => %s",
                    (container == mContainer) ? "consistent" : "inconsistent"));

            if (((mEmpty == empty) && (mContainer == container)) ||
                ((mEmpty == eDontCare) && (mContainer == container)) ||
                ((mContainer == eDontCare) && (mEmpty == empty)))
            {
                Element* element =
                    nsRDFConInstanceTestNode::Element::Create(valueres, container, empty);

                if (! element)
                    return NS_ERROR_OUT_OF_MEMORY;

                inst->AddSupportingElement(element);
            }
            else {
                aInstantiations.Erase(inst--);
            }
        }
    }

    return NS_OK;
}

bool
nsRDFConInstanceTestNode::CanPropagate(nsIRDFResource* aSource,
                                       nsIRDFResource* aProperty,
                                       nsIRDFNode* aTarget,
                                       Instantiation& aInitialBindings) const
{
    nsresult rv;

    bool canpropagate = false;

    nsCOMPtr<nsIRDFContainerUtils> rdfc
        = do_GetService("@mozilla.org/rdf/container-utils;1");

    if (! rdfc)
        return PR_FALSE;

    // We can certainly propagate ordinal properties
    rv = rdfc->IsOrdinalProperty(aProperty, &canpropagate);
    if (NS_FAILED(rv)) return PR_FALSE;

    if (! canpropagate) {
        canpropagate = mProcessor->ContainmentProperties().Contains(aProperty);
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
               ("nsRDFConInstanceTestNode[%p]: CanPropagate([%s]==[%s]=>[%s]) => %s",
                this, source, property, NS_ConvertUTF16toUTF8(target).get(),
                canpropagate ? "true" : "false"));
    }
#endif

    if (canpropagate) {
        aInitialBindings.AddAssignment(mContainerVariable, aSource);
        return PR_TRUE;
    }

    return PR_FALSE;
}

void
nsRDFConInstanceTestNode::Retract(nsIRDFResource* aSource,
                                  nsIRDFResource* aProperty,
                                  nsIRDFNode* aTarget) const
{
    // XXXwaterson oof. complicated. figure this out.
    if (0) {
        mProcessor->RetractElement(Element(aSource, mContainer, mEmpty));
    }
}

