/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXULTemplateQueryProcessorStorage_h__
#define nsXULTemplateQueryProcessorStorage_h__

#include "nsIXULTemplateBuilder.h"
#include "nsIXULTemplateQueryProcessor.h"

#include "nsISimpleEnumerator.h"
#include "nsCOMArray.h"
#include "nsIVariant.h"

#include "mozIStorageValueArray.h"
#include "mozIStorageStatement.h"
#include "mozIStorageConnection.h"

class nsXULTemplateQueryProcessorStorage;

class nsXULTemplateResultSetStorage : public nsISimpleEnumerator
{
private:

    nsCOMPtr<mozIStorageStatement> mStatement;

    nsCOMArray<nsIAtom> mColumnNames;

public:

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsISimpleEnumerator interface
    NS_DECL_NSISIMPLEENUMERATOR

    nsXULTemplateResultSetStorage(mozIStorageStatement* aStatement);

    PRInt32 GetColumnIndex(nsIAtom* aColumnName);

    void FillColumnValues(nsCOMArray<nsIVariant>& aArray);

};

class nsXULTemplateQueryProcessorStorage : public nsIXULTemplateQueryProcessor
{
public:

    nsXULTemplateQueryProcessorStorage();

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIXULTemplateQueryProcessor interface
    NS_DECL_NSIXULTEMPLATEQUERYPROCESSOR

private:

    nsCOMPtr<mozIStorageConnection> mStorageConnection;
    bool mGenerationStarted;
};

#endif // nsXULTemplateQueryProcessorStorage_h__
