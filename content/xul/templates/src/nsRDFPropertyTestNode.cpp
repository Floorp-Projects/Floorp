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

#include "nsRDFPropertyTestNode.h"
#include "nsConflictSet.h"
#include "nsString.h"

#include "prlog.h"
#ifdef PR_LOGGING
extern PRLogModuleInfo* gXULTemplateLog;
#include "nsIRDFLiteral.h"
#include "nsXULContentUtils.h"
#endif

nsRDFPropertyTestNode::nsRDFPropertyTestNode(InnerNode* aParent,
                                             nsConflictSet& aConflictSet,
                                             nsIRDFDataSource* aDataSource,
                                             PRInt32 aSourceVariable,
                                             nsIRDFResource* aProperty,
                                             PRInt32 aTargetVariable)
    : nsRDFTestNode(aParent),
      mConflictSet(aConflictSet),
      mDataSource(aDataSource),
      mSourceVariable(aSourceVariable),
      mSource(nsnull),
      mProperty(aProperty),
      mTargetVariable(aTargetVariable),
      mTarget(nsnull)
{
#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
        const char* prop = "(null)";
        if (aProperty)
            aProperty->GetValueConst(&prop);

        PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
               ("nsRDFPropertyTestNode[%p]: parent=%p source=%d property=%s target=%d",
                this, aParent, aSourceVariable, prop, aTargetVariable));
    }
#endif
}


nsRDFPropertyTestNode::nsRDFPropertyTestNode(InnerNode* aParent,
                                             nsConflictSet& aConflictSet,
                                             nsIRDFDataSource* aDataSource,
                                             nsIRDFResource* aSource,
                                             nsIRDFResource* aProperty,
                                             PRInt32 aTargetVariable)
    : nsRDFTestNode(aParent),
      mConflictSet(aConflictSet),
      mDataSource(aDataSource),
      mSourceVariable(0),
      mSource(aSource),
      mProperty(aProperty),
      mTargetVariable(aTargetVariable),
      mTarget(nsnull)
{
#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
        const char* source = "(null)";
        if (aSource)
            aSource->GetValueConst(&source);

        const char* prop = "(null)";
        if (aProperty)
            aProperty->GetValueConst(&prop);

        PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
               ("nsRDFPropertyTestNode[%p]: parent=%p source=%s property=%s target=%d",
                this, source, prop, aTargetVariable));
    }
#endif
}


nsRDFPropertyTestNode::nsRDFPropertyTestNode(InnerNode* aParent,
                                             nsConflictSet& aConflictSet,
                                             nsIRDFDataSource* aDataSource,
                                             PRInt32 aSourceVariable,
                                             nsIRDFResource* aProperty,
                                             nsIRDFNode* aTarget)
    : nsRDFTestNode(aParent),
      mConflictSet(aConflictSet),
      mDataSource(aDataSource),
      mSourceVariable(aSourceVariable),
      mSource(nsnull),
      mProperty(aProperty),
      mTargetVariable(0),
      mTarget(aTarget)
{
#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
        const char* prop = "(null)";
        if (aProperty)
            aProperty->GetValueConst(&prop);

        nsAutoString target;
        nsXULContentUtils::GetTextForNode(aTarget, target);

        PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
               ("nsRDFPropertyTestNode[%p]: parent=%p source=%s property=%s target=%d",
                this, aSourceVariable, prop, NS_ConvertUCS2toUTF8(target).get()));
    }
#endif
}


nsresult
nsRDFPropertyTestNode::FilterInstantiations(InstantiationSet& aInstantiations, void* aClosure) const
{
    nsresult rv;

    InstantiationSet::Iterator last = aInstantiations.Last();
    for (InstantiationSet::Iterator inst = aInstantiations.First(); inst != last; ++inst) {
        PRBool hasSourceBinding;
        Value sourceValue;

        if (mSource) {
            hasSourceBinding = PR_TRUE;
            sourceValue = mSource;
        }
        else {
            hasSourceBinding = inst->mAssignments.GetAssignmentFor(mSourceVariable, &sourceValue);
        }

        PRBool hasTargetBinding;
        Value targetValue;

        if (mTarget) {
            hasTargetBinding = PR_TRUE;
            targetValue = mTarget;
        }
        else {
            hasTargetBinding = inst->mAssignments.GetAssignmentFor(mTargetVariable, &targetValue);
        }

#ifdef PR_LOGGING
        if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
            const char* source = "(unbound)";
            if (hasSourceBinding)
                VALUE_TO_IRDFRESOURCE(sourceValue)->GetValueConst(&source);

            nsAutoString target(NS_LITERAL_STRING("(unbound)"));
            if (hasTargetBinding)
                nsXULContentUtils::GetTextForNode(VALUE_TO_IRDFNODE(targetValue), target);

            PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
                   ("nsRDFPropertyTestNode[%p]: FilterInstantiations() source=[%s] target=[%s]",
                    this, source, NS_ConvertUCS2toUTF8(target).get()));
        }
#endif

        if (hasSourceBinding && hasTargetBinding) {
            // it's a consistency check. see if we have a assignment that is consistent
            PRBool hasAssertion;
            rv = mDataSource->HasAssertion(VALUE_TO_IRDFRESOURCE(sourceValue),
                                           mProperty,
                                           VALUE_TO_IRDFNODE(targetValue),
                                           PR_TRUE,
                                           &hasAssertion);
            if (NS_FAILED(rv)) return rv;

#ifdef PR_LOGGING
            PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
                   ("    consistency check => %s", hasAssertion ? "passed" : "failed"));
#endif

            if (hasAssertion) {
                // it's consistent.
                Element* element =
                    nsRDFPropertyTestNode::Element::Create(mConflictSet.GetPool(),
                                                           VALUE_TO_IRDFRESOURCE(sourceValue),
                                                           mProperty,
                                                           VALUE_TO_IRDFNODE(targetValue));

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
                rv = mDataSource->GetTargets(VALUE_TO_IRDFRESOURCE(sourceValue),
                                             mProperty,
                                             PR_TRUE,
                                             getter_AddRefs(results));
            }
            else {
                rv = mDataSource->GetSources(mProperty,
                                             VALUE_TO_IRDFNODE(targetValue),
                                             PR_TRUE,
                                             getter_AddRefs(results));
                if (NS_FAILED(rv)) return rv;
            }

            while (1) {
                PRBool hasMore;
                rv = results->HasMoreElements(&hasMore);
                if (NS_FAILED(rv)) return rv;

                if (! hasMore)
                    break;

                nsCOMPtr<nsISupports> isupports;
                rv = results->GetNext(getter_AddRefs(isupports));
                if (NS_FAILED(rv)) return rv;

                PRInt32 variable;
                Value value;

                if (hasSourceBinding) {
                    variable = mTargetVariable;

                    nsCOMPtr<nsIRDFNode> target = do_QueryInterface(isupports);
                    NS_ASSERTION(target != nsnull, "target is not an nsIRDFNode");

#ifdef PR_LOGGING
                    if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
                        nsAutoString s(NS_LITERAL_STRING("(none found)"));
                        if (target)
                            nsXULContentUtils::GetTextForNode(target, s);

                        PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
                               ("    target => %s", NS_ConvertUCS2toUTF8(s).get()));
                    }
#endif

                    if (! target) continue;

                    targetValue = value = target.get();
                }
                else {
                    variable = mSourceVariable;

                    nsCOMPtr<nsIRDFResource> source = do_QueryInterface(isupports);
                    NS_ASSERTION(source != nsnull, "source is not an nsIRDFResource");

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

                    sourceValue = value = source.get();
                }

                // Copy the original instantiation, and add it to the
                // instantiation set with the new assignment that we've
                // introduced. Ownership will be transferred to the
                Instantiation newinst = *inst;
                newinst.AddAssignment(variable, value);

                Element* element =
                    nsRDFPropertyTestNode::Element::Create(mConflictSet.GetPool(),
                                                           VALUE_TO_IRDFRESOURCE(sourceValue),
                                                           mProperty,
                                                           VALUE_TO_IRDFNODE(targetValue));

                if (! element)
                    return NS_ERROR_OUT_OF_MEMORY;

                newinst.AddSupportingElement(element);

                aInstantiations.Insert(inst, newinst);
            }

            // finally, remove the "under specified" instantiation.
            aInstantiations.Erase(inst--);
        }
        else {
            // Neither source nor target assignment!
            NS_ERROR("can't do open ended queries like that!");
            return NS_ERROR_UNEXPECTED;
        }
    }

    return NS_OK;
}


nsresult
nsRDFPropertyTestNode::GetAncestorVariables(VariableSet& aVariables) const
{
    nsresult rv;

    if (mSourceVariable) {
        rv = aVariables.Add(mSourceVariable);
        if (NS_FAILED(rv)) return rv;
    }

    if (mTargetVariable) {
        rv = aVariables.Add(mTargetVariable);
        if (NS_FAILED(rv)) return rv;
    }

    return TestNode::GetAncestorVariables(aVariables);
}

PRBool
nsRDFPropertyTestNode::CanPropagate(nsIRDFResource* aSource,
                                    nsIRDFResource* aProperty,
                                    nsIRDFNode* aTarget,
                                    Instantiation& aInitialBindings) const
{
    PRBool result;

    if ((mProperty.get() != aProperty) ||
        (mSource && mSource.get() != aSource) ||
        (mTarget && mTarget.get() != aTarget)) {
        result = PR_FALSE;
    }
    else {
        if (mSourceVariable)
            aInitialBindings.AddAssignment(mSourceVariable, Value(aSource));

        if (mTargetVariable)
            aInitialBindings.AddAssignment(mTargetVariable, Value(aTarget));

        result = PR_TRUE;
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
                this, source, property, NS_ConvertUCS2toUTF8(target).get(),
                result ? "true" : "false"));
    }
#endif

    return result;
}

void
nsRDFPropertyTestNode::Retract(nsIRDFResource* aSource,
                               nsIRDFResource* aProperty,
                               nsIRDFNode* aTarget,
                               nsTemplateMatchSet& aFirings,
                               nsTemplateMatchSet& aRetractions) const
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
                    this, source, property, NS_ConvertUCS2toUTF8(target).get()));
        }
#endif

        mConflictSet.Remove(Element(aSource, aProperty, aTarget), aFirings, aRetractions);
    }
}

