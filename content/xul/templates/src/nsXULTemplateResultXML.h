/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXULTemplateResultXML_h__
#define nsXULTemplateResultXML_h__

#include "nsCOMPtr.h"
#include "nsIURI.h"
#include "nsIRDFResource.h"
#include "nsXULTemplateQueryProcessorXML.h"
#include "nsIXULTemplateResult.h"
#include "mozilla/Attributes.h"

/**
 * An single result of an query
 */
class nsXULTemplateResultXML MOZ_FINAL : public nsIXULTemplateResult
{
public:
    NS_DECL_ISUPPORTS

    NS_DECL_NSIXULTEMPLATERESULT

    nsXULTemplateResultXML(nsXMLQuery* aQuery,
                           nsIDOMNode* aNode,
                           nsXMLBindingSet* aBindings);

    void GetNode(nsIDOMNode** aNode);

protected:

    ~nsXULTemplateResultXML() {}

    // ID used for persisting data. It is constructed using the mNode's
    // base uri plus the node's id to form 'baseuri#id'. If the node has no
    // id, then an id of the form 'row<some number>' is generated. In the
    // latter case, persistence will not work as there won't be a unique id.
    nsAutoString mId;

    // query that generated the result
    nsCOMPtr<nsXMLQuery> mQuery;

    // context node in datasource
    nsCOMPtr<nsIDOMNode> mNode;

    // assignments in query
    nsXMLBindingValues mRequiredValues;

    // extra assignments made by rules (<binding> tags)
    nsXMLBindingValues mOptionalValues;
};

#endif // nsXULTemplateResultXML_h__
