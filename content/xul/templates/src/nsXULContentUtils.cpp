/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 03/27/2000   IBM Corp.       Added PR_CALLBACK for Optlink
 *                               use in OS2
 */


/*

  A package of routines shared by the XUL content code.

 */

#include "mozilla/Util.h"

#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMXULCommandDispatcher.h"
#include "nsIDOMXULDocument.h"
#include "nsIRDFNode.h"
#include "nsINameSpaceManager.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsXULContentUtils.h"
#include "nsLayoutCID.h"
#include "nsNetUtil.h"
#include "nsRDFCID.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsGkAtoms.h"
#include "prlog.h"
#include "prtime.h"
#include "rdf.h"
#include "nsContentUtils.h"
#include "nsIDateTimeFormat.h"
#include "nsDateTimeFormatCID.h"
#include "nsIScriptableDateFormat.h"
#include "nsICollation.h"
#include "nsCollationCID.h"
#include "nsILocale.h"
#include "nsILocaleService.h"
#include "nsIConsoleService.h"
#include "nsEscape.h"

using namespace mozilla;

//------------------------------------------------------------------------

nsIRDFService* nsXULContentUtils::gRDF;
nsIDateTimeFormat* nsXULContentUtils::gFormat;
nsICollation *nsXULContentUtils::gCollation;

#ifdef PR_LOGGING
extern PRLogModuleInfo* gXULTemplateLog;
#endif

#define XUL_RESOURCE(ident, uri) nsIRDFResource* nsXULContentUtils::ident
#define XUL_LITERAL(ident, val) nsIRDFLiteral* nsXULContentUtils::ident
#include "nsXULResourceList.h"
#undef XUL_RESOURCE
#undef XUL_LITERAL

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

#define XUL_RESOURCE(ident, uri)                              \
  PR_BEGIN_MACRO                                              \
   rv = gRDF->GetResource(NS_LITERAL_CSTRING(uri), &(ident)); \
   if (NS_FAILED(rv)) return rv;                              \
  PR_END_MACRO

#define XUL_LITERAL(ident, val)                                   \
  PR_BEGIN_MACRO                                                  \
   rv = gRDF->GetLiteral(NS_LITERAL_STRING(val).get(), &(ident)); \
   if (NS_FAILED(rv)) return rv;                                  \
  PR_END_MACRO

#include "nsXULResourceList.h"
#undef XUL_RESOURCE
#undef XUL_LITERAL

    rv = CallCreateInstance(NS_DATETIMEFORMAT_CONTRACTID, &gFormat);
    if (NS_FAILED(rv)) {
        return rv;
    }

    return NS_OK;
}


nsresult
nsXULContentUtils::Finish()
{
    NS_IF_RELEASE(gRDF);

#define XUL_RESOURCE(ident, uri) NS_IF_RELEASE(ident)
#define XUL_LITERAL(ident, val) NS_IF_RELEASE(ident)
#include "nsXULResourceList.h"
#undef XUL_RESOURCE
#undef XUL_LITERAL

    NS_IF_RELEASE(gFormat);
    NS_IF_RELEASE(gCollation);

    return NS_OK;
}

nsICollation*
nsXULContentUtils::GetCollation()
{
    if (!gCollation) {
        nsresult rv;

        // get a locale service 
        nsCOMPtr<nsILocaleService> localeService =
            do_GetService(NS_LOCALESERVICE_CONTRACTID, &rv);
        if (NS_SUCCEEDED(rv)) {
            nsCOMPtr<nsILocale> locale;
            rv = localeService->GetApplicationLocale(getter_AddRefs(locale));
            if (NS_SUCCEEDED(rv) && locale) {
                nsCOMPtr<nsICollationFactory> colFactory =
                    do_CreateInstance(NS_COLLATIONFACTORY_CONTRACTID);
                if (colFactory) {
                    rv = colFactory->CreateCollation(locale, &gCollation);
                    NS_ASSERTION(NS_SUCCEEDED(rv),
                                 "couldn't create collation instance");
                } else
                    NS_ERROR("couldn't create instance of collation factory");
            } else
                NS_ERROR("unable to get application locale");
        } else
            NS_ERROR("couldn't get locale factory");
    }

    return gCollation;
}

//------------------------------------------------------------------------

nsresult
nsXULContentUtils::FindChildByTag(nsIContent* aElement,
                                  int32_t aNameSpaceID,
                                  nsIAtom* aTag,
                                  nsIContent** aResult)
{
    for (nsIContent* child = aElement->GetFirstChild();
         child;
         child = child->GetNextSibling()) {

        if (child->NodeInfo()->Equals(aTag, aNameSpaceID)) {
            NS_ADDREF(*aResult = child);

            return NS_OK;
        }
    }

    *aResult = nullptr;
    return NS_RDF_NO_VALUE; // not found
}


nsresult
nsXULContentUtils::GetElementResource(nsIContent* aElement, nsIRDFResource** aResult)
{
    // Perform a reverse mapping from an element in the content model
    // to an RDF resource.
    nsresult rv;

    PRUnichar buf[128];
    nsFixedString id(buf, ArrayLength(buf), 0);

    // Whoa.  Why the "id" attribute?  What if it's not even a XUL
    // element?  This is totally bogus!
    aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::id, id);
    if (id.IsEmpty())
        return NS_ERROR_FAILURE;

    // Since the element will store its ID attribute as a document-relative value,
    // we may need to qualify it first...
    nsCOMPtr<nsIDocument> doc = aElement->GetDocument();
    NS_ASSERTION(doc, "element is not in any document");
    if (! doc)
        return NS_ERROR_FAILURE;

    rv = nsXULContentUtils::MakeElementResource(doc, id, aResult);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


/*
	Note: this routine is similar, yet distinctly different from, nsBookmarksService::GetTextForNode
*/

nsresult
nsXULContentUtils::GetTextForNode(nsIRDFNode* aNode, nsAString& aResult)
{
    if (! aNode) {
        aResult.Truncate();
        return NS_OK;
    }

    nsresult rv;

    // Literals are the most common, so try these first.
    nsCOMPtr<nsIRDFLiteral> literal = do_QueryInterface(aNode);
    if (literal) {
        const PRUnichar* p;
        rv = literal->GetValueConst(&p);
        if (NS_FAILED(rv)) return rv;

        aResult = p;
        return NS_OK;
    }

    nsCOMPtr<nsIRDFDate> dateLiteral = do_QueryInterface(aNode);
    if (dateLiteral) {
        PRTime value;
        rv = dateLiteral->GetValue(&value);
        if (NS_FAILED(rv)) return rv;

        nsAutoString str;
        rv = gFormat->FormatPRTime(nullptr /* nsILocale* locale */,
                                  kDateFormatShort,
                                  kTimeFormatSeconds,
                                  value,
                                  str);
        aResult.Assign(str);

        if (NS_FAILED(rv)) return rv;

        return NS_OK;
    }

    nsCOMPtr<nsIRDFInt> intLiteral = do_QueryInterface(aNode);
    if (intLiteral) {
        int32_t	value;
        rv = intLiteral->GetValue(&value);
        if (NS_FAILED(rv)) return rv;

        aResult.Truncate();
        nsAutoString intStr;
        intStr.AppendInt(value, 10);
        aResult.Append(intStr);
        return NS_OK;
    }


    nsCOMPtr<nsIRDFResource> resource = do_QueryInterface(aNode);
    if (resource) {
        const char* p;
        rv = resource->GetValueConst(&p);
        if (NS_FAILED(rv)) return rv;
        CopyUTF8toUTF16(p, aResult);
        return NS_OK;
    }

    NS_ERROR("not a resource or a literal");
    return NS_ERROR_UNEXPECTED;
}

nsresult
nsXULContentUtils::MakeElementURI(nsIDocument* aDocument,
                                  const nsAString& aElementID,
                                  nsCString& aURI)
{
    // Convert an element's ID to a URI that can be used to refer to
    // the element in the XUL graph.

    nsIURI *docURI = aDocument->GetDocumentURI();
    NS_ENSURE_TRUE(docURI, NS_ERROR_UNEXPECTED);

    nsRefPtr<nsIURI> docURIClone;
    nsresult rv = docURI->Clone(getter_AddRefs(docURIClone));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = docURIClone->SetRef(NS_ConvertUTF16toUTF8(aElementID));
    if (NS_SUCCEEDED(rv)) {
        return docURIClone->GetSpec(aURI);
    }

    // docURIClone is apparently immutable. Fine - we can append ref manually.
    rv = docURI->GetSpec(aURI);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoCString ref;
    NS_EscapeURL(NS_ConvertUTF16toUTF8(aElementID), esc_FilePath | esc_AlwaysCopy, ref);

    aURI.Append('#');
    aURI.Append(ref);

    return NS_OK;
}


nsresult
nsXULContentUtils::MakeElementResource(nsIDocument* aDocument, const nsAString& aID, nsIRDFResource** aResult)
{
    nsresult rv;

    char buf[256];
    nsFixedCString uri(buf, sizeof(buf), 0);
    rv = MakeElementURI(aDocument, aID, uri);
    if (NS_FAILED(rv)) return rv;

    rv = gRDF->GetResource(uri, aResult);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create resource");
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}



nsresult
nsXULContentUtils::MakeElementID(nsIDocument* aDocument,
                                 const nsACString& aURI,
                                 nsAString& aElementID)
{
    // Convert a URI into an element ID that can be accessed from the
    // DOM APIs.
    nsCOMPtr<nsIURI> uri;
    nsresult rv = NS_NewURI(getter_AddRefs(uri), aURI,
                            aDocument->GetDocumentCharacterSet().get());
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoCString ref;
    uri->GetRef(ref);
    CopyUTF8toUTF16(ref, aElementID);

    return NS_OK;
}

nsresult
nsXULContentUtils::GetResource(int32_t aNameSpaceID, nsIAtom* aAttribute, nsIRDFResource** aResult)
{
    // construct a fully-qualified URI from the namespace/tag pair.
    NS_PRECONDITION(aAttribute != nullptr, "null ptr");
    if (! aAttribute)
        return NS_ERROR_NULL_POINTER;

    return GetResource(aNameSpaceID, nsDependentAtomString(aAttribute),
                       aResult);
}


nsresult
nsXULContentUtils::GetResource(int32_t aNameSpaceID, const nsAString& aAttribute, nsIRDFResource** aResult)
{
    // construct a fully-qualified URI from the namespace/tag pair.

    // XXX should we allow nodes with no namespace???
    //NS_PRECONDITION(aNameSpaceID != kNameSpaceID_Unknown, "no namespace");
    //if (aNameSpaceID == kNameSpaceID_Unknown)
    //    return NS_ERROR_UNEXPECTED;

    nsresult rv;

    PRUnichar buf[256];
    nsFixedString uri(buf, ArrayLength(buf), 0);
    if (aNameSpaceID != kNameSpaceID_Unknown && aNameSpaceID != kNameSpaceID_None) {
        rv = nsContentUtils::NameSpaceManager()->GetNameSpaceURI(aNameSpaceID, uri);
        // XXX ignore failure; treat as "no namespace"
    }

    // XXX check to see if we need to insert a '/' or a '#'. Oy.
    if (!uri.IsEmpty()  && uri.Last() != '#' && uri.Last() != '/' && aAttribute.First() != '#')
        uri.Append(PRUnichar('#'));

    uri.Append(aAttribute);

    rv = gRDF->GetUnicodeResource(uri, aResult);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource");
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


nsresult
nsXULContentUtils::SetCommandUpdater(nsIDocument* aDocument, nsIContent* aElement)
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

    nsCOMPtr<nsIDOMXULDocument> xuldoc = do_QueryInterface(aDocument);
    NS_ASSERTION(xuldoc != nullptr, "not a xul document");
    if (! xuldoc)
        return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIDOMXULCommandDispatcher> dispatcher;
    rv = xuldoc->GetCommandDispatcher(getter_AddRefs(dispatcher));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get dispatcher");
    if (NS_FAILED(rv)) return rv;

    NS_ASSERTION(dispatcher != nullptr, "no dispatcher");
    if (! dispatcher)
        return NS_ERROR_UNEXPECTED;

    nsAutoString events;
    aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::events, events);
    if (events.IsEmpty())
        events.AssignLiteral("*");

    nsAutoString targets;
    aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::targets, targets);

    if (targets.IsEmpty())
        targets.AssignLiteral("*");

    nsCOMPtr<nsIDOMElement> domelement = do_QueryInterface(aElement);
    NS_ASSERTION(domelement != nullptr, "not a DOM element");
    if (! domelement)
        return NS_ERROR_UNEXPECTED;

    rv = dispatcher->AddCommandUpdater(domelement, events, targets);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

void
nsXULContentUtils::LogTemplateError(const char* aStr)
{
  nsAutoString message;
  message.AssignLiteral("Error parsing template: ");
  message.Append(NS_ConvertUTF8toUTF16(aStr).get());

  nsCOMPtr<nsIConsoleService> cs = do_GetService(NS_CONSOLESERVICE_CONTRACTID);
  if (cs) {
    cs->LogStringMessage(message.get());
    PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS, ("Error parsing template: %s", aStr));
  }
}
