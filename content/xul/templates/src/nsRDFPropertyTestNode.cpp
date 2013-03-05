/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRDFPropertyTestNode.h"
#include "nsString.h"
#include "nsXULContentUtils.h"

#include "prlog.h"
#ifdef PR_LOGGING
extern PRLogModuleInfo* gXULTemplateLog;
#include "nsIRDFLiteral.h"
#endif

nsRDFPropertyTestNode::nsRDFPropertyTestNode(TestNode* aParent,
                                             nsXULTemplateQueryProcessorRDF* aProcessor,
                                             nsIAtom* aSourceVariable,
                                             nsIRDFResource* aProperty,
                                             nsIAtom* aTargetVariable)
    : nsRDFTestNode(aParent),
      mProcessor(aProcessor),
      mSourceVariable(aSourceVariable),
      mSource(nullptr),
      mProperty(aProperty),
      mTargetVariable(aTargetVariable),
      mTarget(nullptr)
{
#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
        const char* prop = "(null)";
        if (aProperty)
            aProperty->GetValueConst(&prop);

        nsAutoString svar(NS_LITERAL_STRING("(none)"));
        if (mSourceVariable)
            mSourceVariable->ToString(svar);

        nsAutoString tvar(NS_LITERAL_STRING("(none)"));
        if (mTargetVariable)
            mTargetVariable->ToString(tvar);

        PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
               ("nsRDFPropertyTestNode[%p]: parent=%p source=%s property=%s target=%s",
                this, aParent, NS_ConvertUTF16toUTF8(svar).get(), prop, NS_ConvertUTF16toUTF8(tvar).get()));
    }
#endif
}


nsRDFPropertyTestNode::nsRDFPropertyTestNode(TestNode* aParent,
                                             nsXULTemplateQueryProcessorRDF* aProcessor,
                                             nsIRDFResource* aSource,
                                             nsIRDFResource* aProperty,
                                             nsIAtom* aTargetVariable)
    : nsRDFTestNode(aParent),
      mProcessor(aProcessor),
      mSourceVariable(0),
      mSource(aSource),
      mProperty(aProperty),
      mTargetVariable(aTargetVariable),
      mTarget(nullptr)
{
#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
        const char* source = "(null)";
        if (aSource)
            aSource->GetValueConst(&source);

        const char* prop = "(null)";
        if (aProperty)
            aProperty->GetValueConst(&prop);

        nsAutoString tvar(NS_LITERAL_STRING("(none)"));
        if (mTargetVariable)
            mTargetVariable->ToString(tvar);

        PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
               ("nsRDFPropertyTestNode[%p]: parent=%p source=%s property=%s target=%s",
                this, aParent, source, prop, NS_ConvertUTF16toUTF8(tvar).get()));
    }
#endif
}


nsRDFPropertyTestNode::nsRDFPropertyTestNode(TestNode* aParent,
                                             nsXULTemplateQueryProcessorRDF* aProcessor,
                                             nsIAtom* aSourceVariable,
                                             nsIRDFResource* aProperty,
                                             nsIRDFNode* aTarget)
    : nsRDFTestNode(aParent),
      mProcessor(aProcessor),
      mSourceVariable(aSourceVariable),
      mSource(nullptr),
      mProperty(aProperty),
      mTargetVariable(0),
      mTarget(aTarget)
{
#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
        nsAutoString svar(NS_LITERAL_STRING("(none)"));
        if (mSourceVariable)
            mSourceVariable->ToString(svar);

        const char* prop = "(null)";
        if (aProperty)
            aProperty->GetValueConst(&prop);

        nsAutoString target;
        nsXULContentUtils::GetTextForNode(aTarget, target);

        PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
               ("nsRDFPropertyTestNode[%p]: parent=%p source=%s property=%s target=%s",
                this, aParent, NS_ConvertUTF16toUTF8(svar).get(), prop, NS_ConvertUTF16toUTF8(target).get()));
    }
#endif
}


nsresult
nsRDFPropertyTestNode::FilterInstantiations(InstantiationSet& aInstantiations,
                                            bool* aCantHandleYet) const
{
    nsresult rv;

    if (aCantHandleYet)
        *aCantHandleYet = false;

    nsIRDFDataSource* ds = mProcessor->GetDataSource();

    InstantiationSet::Iterator last = aInstantiations.Last();
    for (InstantiationSet::Iterator inst = aInstantiations.First(); inst != last; ++inst) {
        bool hasSourceBinding;
        nsCOMPtr<nsIRDFResource> sourceRes;

        if (mSource) {
            hasSourceBinding = true;
            sourceRes = mSource;
        }
        else {
            nsCOMPtr<nsIRDFNode> sourceValue;
            hasSourceBinding = inst->mAssignments.GetAssignmentFor(mSourceVariable,
                                                                   getter_AddRefs(sourceValue));
            sourceRes = do_QueryInterface(sourceValue);
        }

        bool hasTargetBinding;
        nsCOMPtr<nsIRDFNode> targetValue;

        if (mTarget) {
            hasTargetBinding = true;
            targetValue = mTarget;
        }
        else {
            hasTargetBinding = inst->mAssignments.GetAssignmentFor(mTargetVariable,
                                                                   getter_AddRefs(targetValue));
        }

#ifdef PR_LOGGING
        if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
            const char* source = "(unbound)";
            if (hasSourceBinding)
                sourceRes->GetValueConst(&source);

            nsAutoString target(NS_LITERAL_STRING("(unbound)"));
            if (hasTargetBinding)
                nsXULContentUtils::GetTextForNode(targetValue, target);

            PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
                   ("nsRDFPropertyTestNode[%p]: FilterInstantiations() source=[%s] target=[%s]",
                    this, source, NS_ConvertUTF16toUTF8(target).get()));
        }
#endif

        if (hasSourceBinding && hasTargetBinding) {
            // it's a consistency check. see if we have a assignment that is consistent
            bool hasAssertion;
            rv = ds->HasAssertion(sourceRes, mProperty, targetValue,
                                  true, &hasAssertion);
            if (NS_FAILED(rv)) return rv;

#ifdef PR_LOGGING
            PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
                   ("    consistency check => %s", hasAssertion ? "passed" : "failed"));
#endif

            if (hasAssertion) {
                // it's consistent.
                Element* element =
                    new nsRDFPropertyTestNode::Element(sourceRes, mProperty,
                                                       targetValue);

                if (! element)
                    return NS_ERROR_OUT_OF_MEMORY;

                inst->AddSupportingElement(element);
            }
            else {
                // it's inconsistent. remove it.
                aInstantiations.Erase(inst--);
            }
        }
        else if ((hasSourceBinding && ! hasTargetBinding) ||
                 (! hasSourceBinding && hasTargetBinding)) {
            // it's an open ended query on the source or
            // target. figure out what matches and add as a
            // cross-product.
            nsCOMPtr<nsISimpleEnumerator> results;
            if (hasSourceBinding) {
                rv = ds->GetTargets(sourceRes,
                                    mProperty,
                                    true,
                                    getter_AddRefs(results));
            }
            else {
                rv = ds->GetSources(mProperty,
                                    targetValue,
                                    true,
                                    getter_AddRefs(results));
                if (NS_FAILED(rv)) return rv;
            }

            while (1) {
                bool hasMore;
                rv = results->HasMoreElements(&hasMore);
                if (NS_FAILED(rv)) return rv;

                if (! hasMore)
                    break;

                nsCOMPtr<nsISupports> isupports;
                rv = results->GetNext(getter_AddRefs(isupports));
                if (NS_FAILED(rv)) return rv;

                nsIAtom* variable;
                nsCOMPtr<nsIRDFNode> value;

                if (hasSourceBinding) {
                    variable = mTargetVariable;

                    value = do_QueryInterface(isupports);
                    NS_ASSERTION(value != nullptr, "target is not an nsIRDFNode");

#ifdef PR_LOGGING
                    if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
                        nsAutoString s(NS_LITERAL_STRING("(none found)"));
                        if (value)
                            nsXULContentUtils::GetTextForNode(value, s);

                        PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
                               ("    target => %s", NS_ConvertUTF16toUTF8(s).get()));
                    }
#endif

                    if (! value) continue;

                    targetValue = value;
                }
                else {
                    variable = mSourceVariable;

                    nsCOMPtr<nsIRDFResource> source = do_QueryInterface(isupports);
                    NS_ASSERTION(source != nullptr, "source is not an nsIRDFResource");

#ifdef PR_LOGGING
                    if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
                        const char* s = "(none found)";
                        if (source)
                            source->GetValueConst(&s);

                        PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
                               ("    source => %s", s));
                    }
#endif

                    if (! source) continue;

                    value = sourceRes = source;
                }

                // Copy the original instantiation, and add it to the
                // instantiation set with the new assignment that we've
                // introduced. Ownership will be transferred to the
                Instantiation newinst = *inst;
                newinst.AddAssignment(variable, value);

                Element* element =
                    new nsRDFPropertyTestNode::Element(sourceRes, mProperty,
                                                       targetValue);

                if (! element)
                    return NS_ERROR_OUT_OF_MEMORY;

                newinst.AddSupportingElement(element);

                aInstantiations.Insert(inst, newinst);
            }

            // finally, remove the "under specified" instantiation.
            aInstantiations.Erase(inst--);
        }
        else {
            if (!aCantHandleYet) {
                nsXULContentUtils::LogTemplateError(ERROR_TEMPLATE_TRIPLE_UNBOUND);
                // Neither source nor target assignment!
                return NS_ERROR_UNEXPECTED;
            }

            *aCantHandleYet = true;
            return NS_OK;
        }
    }

    return NS_OK;
}

bool
nsRDFPropertyTestNode::CanPropagate(nsIRDFResource* aSource,
                                    nsIRDFResource* aProperty,
                                    nsIRDFNode* aTarget,
                                    Instantiation& aInitialBindings) const
{
    bool result;

    if ((mProperty.get() != aProperty) ||
        (mSource && mSource.get() != aSource) ||
        (mTarget && mTarget.get() != aTarget)) {
        result = false;
    }
    else {
        if (mSourceVariable)
            aInitialBindings.AddAssignment(mSourceVariable, aSource);

        if (mTargetVariable)
            aInitialBindings.AddAssignment(mTargetVariable, aTarget);

        result = true;
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
               ("nsRDFPropertyTestNode[%p]: CanPropagate([%s]==[%s]=>[%s]) => %s",
                this, source, property, NS_ConvertUTF16toUTF8(target).get(),
                result ? "true" : "false"));
    }
#endif

    return result;
}

void
nsRDFPropertyTestNode::Retract(nsIRDFResource* aSource,
                               nsIRDFResource* aProperty,
                               nsIRDFNode* aTarget) const
{
    if (aProperty == mProperty.get()) {
#ifdef PR_LOGGING
        if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
            const char* source;
            aSource->GetValueConst(&source);

            const char* property;
            aProperty->GetValueConst(&property);

            nsAutoString target;
            nsXULContentUtils::GetTextForNode(aTarget, target);

            PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
                   ("nsRDFPropertyTestNode[%p]: Retract([%s]==[%s]=>[%s])",
                    this, source, property, NS_ConvertUTF16toUTF8(target).get()));
        }
#endif

        mProcessor->RetractElement(Element(aSource, aProperty, aTarget));
    }
}

