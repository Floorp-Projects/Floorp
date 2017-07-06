/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXULTemplateQueryProcessorXML_h__
#define nsXULTemplateQueryProcessorXML_h__

#include "nsIXULTemplateBuilder.h"
#include "nsIXULTemplateQueryProcessor.h"

#include "nsAutoPtr.h"
#include "nsISimpleEnumerator.h"
#include "nsString.h"
#include "nsCOMArray.h"
#include "nsRefPtrHashtable.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMXPathEvaluator.h"
#include "nsXMLBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIXMLHttpRequest.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/XPathEvaluator.h"
#include "mozilla/dom/XPathResult.h"

class nsXULTemplateQueryProcessorXML;

#define NS_IXMLQUERY_IID \
  {0x0358d692, 0xccce, 0x4a97, \
    { 0xb2, 0x51, 0xba, 0x8f, 0x17, 0x0f, 0x3b, 0x6f }}

class nsXMLQuery final : public nsISupports
{
  public:
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_IXMLQUERY_IID)

    NS_DECL_ISUPPORTS

    // return a weak reference to the processor the query was created from
    nsXULTemplateQueryProcessorXML* Processor() { return mProcessor; }

    // return a weak reference t the member variable for the query
    nsIAtom* GetMemberVariable() { return mMemberVariable; }

    // return a weak reference to the expression used to generate results
    mozilla::dom::XPathExpression* GetResultsExpression()
      { return mResultsExpr; }

    // return a weak reference to the additional required bindings
    nsXMLBindingSet* GetBindingSet() { return mRequiredBindings; }

    // add a required binding for the query
    void
    AddBinding(nsIAtom* aVar, nsAutoPtr<mozilla::dom::XPathExpression>&& aExpr)
    {
        if (!mRequiredBindings) {
            mRequiredBindings = new nsXMLBindingSet();
        }

        mRequiredBindings->AddBinding(aVar, mozilla::Move(aExpr));
    }

    nsXMLQuery(nsXULTemplateQueryProcessorXML* aProcessor,
               nsIAtom* aMemberVariable,
               nsAutoPtr<mozilla::dom::XPathExpression>&& aResultsExpr)
        : mProcessor(aProcessor),
          mMemberVariable(aMemberVariable),
          mResultsExpr(aResultsExpr)
    { }

  protected:
    ~nsXMLQuery() {}

    nsXULTemplateQueryProcessorXML* mProcessor;

    nsCOMPtr<nsIAtom> mMemberVariable;

    nsAutoPtr<mozilla::dom::XPathExpression> mResultsExpr;

    RefPtr<nsXMLBindingSet> mRequiredBindings;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsXMLQuery, NS_IXMLQUERY_IID)

class nsXULTemplateResultSetXML final : public nsISimpleEnumerator
{
private:

    // reference back to the query
    nsCOMPtr<nsXMLQuery> mQuery;

    // the binding set created from <assign> nodes
    RefPtr<nsXMLBindingSet> mBindingSet;

    // set of results contained in this enumerator
    RefPtr<mozilla::dom::XPathResult> mResults;

    // current position within the list of results
    uint32_t mPosition;

    ~nsXULTemplateResultSetXML() {}

public:

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsISimpleEnumerator interface
    NS_DECL_NSISIMPLEENUMERATOR

    nsXULTemplateResultSetXML(nsXMLQuery* aQuery,
                              already_AddRefed<mozilla::dom::XPathResult> aResults,
                              nsXMLBindingSet* aBindingSet)
        : mQuery(aQuery),
          mBindingSet(aBindingSet),
          mResults(aResults),
          mPosition(0)
    {}
};

class nsXULTemplateQueryProcessorXML final : public nsIXULTemplateQueryProcessor,
                                             public nsIDOMEventListener
{
public:

    nsXULTemplateQueryProcessorXML()
        : mGenerationStarted(false)
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
    mozilla::dom::XPathExpression*
    CreateExpression(const nsAString& aExpr,
                     nsINode* aNode,
                     mozilla::ErrorResult& aRv);

private:

    ~nsXULTemplateQueryProcessorXML() {}

    bool mGenerationStarted;

    nsRefPtrHashtable<nsISupportsHashKey, nsXMLBindingSet> mRuleToBindingsMap;

    nsCOMPtr<mozilla::dom::Element> mRoot;

    RefPtr<mozilla::dom::XPathEvaluator> mEvaluator;

    nsCOMPtr<nsIXULTemplateBuilder> mTemplateBuilder;

    nsCOMPtr<nsIXMLHttpRequest> mRequest;
};


#endif // nsXULTemplateQueryProcessorXML_h__
