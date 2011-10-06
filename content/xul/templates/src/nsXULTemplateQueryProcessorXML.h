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
 *    Laurent Jouanneau <laurent.jouanneau@disruptive-innovations.com>
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

#ifndef nsXULTemplateQueryProcessorXML_h__
#define nsXULTemplateQueryProcessorXML_h__

#include "nsIXULTemplateBuilder.h"
#include "nsIXULTemplateQueryProcessor.h"

#include "nsISimpleEnumerator.h"
#include "nsString.h"
#include "nsCOMArray.h"
#include "nsRefPtrHashtable.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMXPathExpression.h"
#include "nsIDOMXPathEvaluator.h"
#include "nsIDOMXPathResult.h"
#include "nsXMLBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIXMLHttpRequest.h"

class nsXULTemplateQueryProcessorXML;

#define NS_IXMLQUERY_IID \
  {0x0358d692, 0xccce, 0x4a97, \
    { 0xb2, 0x51, 0xba, 0x8f, 0x17, 0x0f, 0x3b, 0x6f }}
 
class nsXMLQuery : public nsISupports
{
  public:
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_IXMLQUERY_IID)

    NS_DECL_ISUPPORTS

    // return a weak reference to the processor the query was created from
    nsXULTemplateQueryProcessorXML* Processor() { return mProcessor; }

    // return a weak reference t the member variable for the query
    nsIAtom* GetMemberVariable() { return mMemberVariable; }

    // return a weak reference to the expression used to generate results
    nsIDOMXPathExpression* GetResultsExpression() { return mResultsExpr; }

    // return a weak reference to the additional required bindings
    nsXMLBindingSet* GetBindingSet() { return mRequiredBindings; }

    // add a required binding for the query
    nsresult
    AddBinding(nsIAtom* aVar, nsIDOMXPathExpression* aExpr)
    {
        if (!mRequiredBindings) {
            mRequiredBindings = new nsXMLBindingSet();
            NS_ENSURE_TRUE(mRequiredBindings, NS_ERROR_OUT_OF_MEMORY);
        }

        return mRequiredBindings->AddBinding(aVar, aExpr);
    }

    nsXMLQuery(nsXULTemplateQueryProcessorXML* aProcessor,
                        nsIAtom* aMemberVariable,
                        nsIDOMXPathExpression* aResultsExpr)
        : mProcessor(aProcessor),
          mMemberVariable(aMemberVariable),
          mResultsExpr(aResultsExpr)
    { }

  protected:
    nsXULTemplateQueryProcessorXML* mProcessor;

    nsCOMPtr<nsIAtom> mMemberVariable;

    nsCOMPtr<nsIDOMXPathExpression> mResultsExpr;

    nsRefPtr<nsXMLBindingSet> mRequiredBindings;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsXMLQuery, NS_IXMLQUERY_IID)

class nsXULTemplateResultSetXML : public nsISimpleEnumerator
{
private:

    // reference back to the query
    nsCOMPtr<nsXMLQuery> mQuery;

    // the binding set created from <assign> nodes
    nsRefPtr<nsXMLBindingSet> mBindingSet;

    // set of results contained in this enumerator
    nsCOMPtr<nsIDOMXPathResult> mResults;

    // current position within the list of results
    PRUint32 mPosition;

public:

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsISimpleEnumerator interface
    NS_DECL_NSISIMPLEENUMERATOR

    nsXULTemplateResultSetXML(nsXMLQuery* aQuery,
                              nsIDOMXPathResult* aResults,
                              nsXMLBindingSet* aBindingSet)
        : mQuery(aQuery),
          mBindingSet(aBindingSet),
          mResults(aResults),
          mPosition(0)
    {}
};

class nsXULTemplateQueryProcessorXML : public nsIXULTemplateQueryProcessor,
                                       public nsIDOMEventListener
{
public:

    nsXULTemplateQueryProcessorXML()
        : mGenerationStarted(PR_FALSE)
    {}

    // nsISupports interface
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsXULTemplateQueryProcessorXML,
                                             nsIXULTemplateQueryProcessor)

    // nsIXULTemplateQueryProcessor interface
    NS_DECL_NSIXULTEMPLATEQUERYPROCESSOR

    // nsIDOMEventListener interface
    NS_DECL_NSIDOMEVENTLISTENER

    nsXMLBindingSet*
    GetOptionalBindingsForRule(nsIDOMNode* aRuleNode);

    // create an XPath expression from aExpr, using aNode for
    // resolving namespaces
    nsresult
    CreateExpression(const nsAString& aExpr,
                     nsIDOMNode* aNode,
                     nsIDOMXPathExpression** aCompiledExpr);

private:

    bool mGenerationStarted;

    nsRefPtrHashtable<nsISupportsHashKey, nsXMLBindingSet> mRuleToBindingsMap;

    nsCOMPtr<nsIDOMElement> mRoot;

    nsCOMPtr<nsIDOMXPathEvaluator> mEvaluator;

    nsCOMPtr<nsIXULTemplateBuilder> mTemplateBuilder;

    nsCOMPtr<nsIXMLHttpRequest> mRequest;
};


#endif // nsXULTemplateQueryProcessorXML_h__
