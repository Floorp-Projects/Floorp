/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/*

  A package of routines shared by the XUL content code.

 */

#include "mozilla/ArrayUtils.h"

#include "nsCollationCID.h"
#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "nsICollation.h"
#include "nsIDocument.h"
#include "nsIDOMXULCommandDispatcher.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsXULContentUtils.h"
#include "nsLayoutCID.h"
#include "nsRDFCID.h"
#include "nsString.h"
#include "nsGkAtoms.h"
#include "XULDocument.h"

using namespace mozilla;
using dom::XULDocument;

//------------------------------------------------------------------------

nsIRDFService* nsXULContentUtils::gRDF;
nsICollation *nsXULContentUtils::gCollation;

//------------------------------------------------------------------------
// Constructors n' stuff
//

nsresult
nsXULContentUtils::Init()
{
    static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
    nsresult rv = CallGetService(kRDFServiceCID, &gRDF);
    if (NS_FAILED(rv)) {
        return rv;
    }

    return NS_OK;
}


nsresult
nsXULContentUtils::Finish()
{
    NS_IF_RELEASE(gRDF);
    NS_IF_RELEASE(gCollation);

    return NS_OK;
}

nsICollation*
nsXULContentUtils::GetCollation()
{
    if (!gCollation) {
        nsCOMPtr<nsICollationFactory> colFactory =
            do_CreateInstance(NS_COLLATIONFACTORY_CONTRACTID);
        if (colFactory) {
            DebugOnly<nsresult> rv = colFactory->CreateCollation(&gCollation);
            NS_ASSERTION(NS_SUCCEEDED(rv),
                         "couldn't create collation instance");
        } else
            NS_ERROR("couldn't create instance of collation factory");
    }

    return gCollation;
}


//------------------------------------------------------------------------
//

nsresult
nsXULContentUtils::FindChildByTag(nsIContent* aElement,
                                  int32_t aNameSpaceID,
                                  nsAtom* aTag,
                                  Element** aResult)
{
    for (nsIContent* child = aElement->GetFirstChild();
         child;
         child = child->GetNextSibling()) {

        if (child->IsElement() &&
            child->NodeInfo()->Equals(aTag, aNameSpaceID)) {
            NS_ADDREF(*aResult = child->AsElement());
            return NS_OK;
        }
    }

    *aResult = nullptr;
    return NS_RDF_NO_VALUE; // not found
}

nsresult
nsXULContentUtils::SetCommandUpdater(nsIDocument* aDocument, Element* aElement)
{
    // Deal with setting up a 'commandupdater'. Pulls the 'events' and
    // 'targets' attributes off of aElement, and adds it to the
    // document's command dispatcher.
    NS_PRECONDITION(aDocument != nullptr, "null ptr");
    if (! aDocument)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aElement != nullptr, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    NS_ASSERTION(aDocument->IsXULDocument(), "not a xul document");
    if (! aDocument->IsXULDocument())
        return NS_ERROR_UNEXPECTED;

    XULDocument* xuldoc = aDocument->AsXULDocument();
    nsCOMPtr<nsIDOMXULCommandDispatcher> dispatcher =
        xuldoc->GetCommandDispatcher();
    NS_ASSERTION(dispatcher != nullptr, "no dispatcher");
    if (! dispatcher)
        return NS_ERROR_UNEXPECTED;

    nsAutoString events;
    aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::events, events);
    if (events.IsEmpty())
        events.Assign('*');

    nsAutoString targets;
    aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::targets, targets);

    if (targets.IsEmpty())
        targets.Assign('*');

    rv = dispatcher->AddCommandUpdater(aElement, events, targets);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

