/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXULTemplateResultStorage_h__
#define nsXULTemplateResultStorage_h__

#include "nsXULTemplateQueryProcessorStorage.h"
#include "nsIRDFResource.h"
#include "nsIXULTemplateResult.h"
#include "mozilla/Attributes.h"

/**
 * A single result of a query from mozstorage
 */
class nsXULTemplateResultStorage final : public nsIXULTemplateResult
{
public:
    NS_DECL_ISUPPORTS

    NS_DECL_NSIXULTEMPLATERESULT

    explicit nsXULTemplateResultStorage(nsXULTemplateResultSetStorage* aResultSet);

protected:

    ~nsXULTemplateResultStorage();

    RefPtr<nsXULTemplateResultSetStorage> mResultSet;

    nsCOMArray<nsIVariant> mValues;

    nsCOMPtr<nsIRDFResource> mNode;
};

#endif // nsXULTemplateResultStorage_h__
