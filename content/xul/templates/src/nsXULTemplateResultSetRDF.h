/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXULTemplateResultSetRDF_h__
#define nsXULTemplateResultSetRDF_h__

#include "nsISimpleEnumerator.h"
#include "nsRuleNetwork.h"
#include "nsRDFQuery.h"
#include "nsXULTemplateResultRDF.h"
#include "mozilla/Attributes.h"

class nsXULTemplateQueryProcessorRDF;
class nsXULTemplateResultRDF;

/**
 * An enumerator used to iterate over a set of results.
 */
class nsXULTemplateResultSetRDF MOZ_FINAL : public nsISimpleEnumerator
{
private:
    nsXULTemplateQueryProcessorRDF* mProcessor;

    nsRDFQuery* mQuery;

    const InstantiationSet* mInstantiations;

    nsCOMPtr<nsIRDFResource> mResource;

    InstantiationSet::List *mCurrent;

    bool mCheckedNext;

public:

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsISimpleEnumerator interface
    NS_DECL_NSISIMPLEENUMERATOR

    nsXULTemplateResultSetRDF(nsXULTemplateQueryProcessorRDF *aProcessor,
                              nsRDFQuery* aQuery,
                              const InstantiationSet* aInstantiations)
        : mProcessor(aProcessor),
          mQuery(aQuery),
          mInstantiations(aInstantiations),
          mCurrent(nullptr),
          mCheckedNext(false)
    { }

    ~nsXULTemplateResultSetRDF()
    {
        delete mInstantiations;
    }
};

#endif // nsXULTemplateResultSetRDF_h__
