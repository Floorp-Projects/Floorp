/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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


#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMXULCommandDispatcher.h"
#include "nsIDOMXULDocument.h"
#include "nsIRDFNode.h"
#include "nsINameSpace.h"
#include "nsINameSpaceManager.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsITextContent.h"
#include "nsIURL.h"
#include "nsIXMLContent.h"
#include "nsXULContentUtils.h"
#include "nsIXULPrototypeCache.h"
#include "nsLayoutCID.h"
#include "nsNetUtil.h"
#include "nsRDFCID.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsXULAtoms.h"
#include "prlog.h"
#include "prtime.h"
#include "rdf.h"

#include "nsIDateTimeFormat.h"
#include "nsDateTimeFormatCID.h"
#include "nsIScriptableDateFormat.h"


static NS_DEFINE_CID(kDateTimeFormatCID,    NS_DATETIMEFORMAT_CID);
static NS_DEFINE_CID(kDateTimeFormatIID,    NS_IDATETIMEFORMAT_IID);
static NS_DEFINE_CID(kNameSpaceManagerCID,  NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_CID(kRDFServiceCID,        NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kTextNodeCID,          NS_TEXTNODE_CID);

//------------------------------------------------------------------------

nsrefcnt nsXULContentUtils::gRefCnt;
nsIRDFService* nsXULContentUtils::gRDF;
nsINameSpaceManager* nsXULContentUtils::gNameSpaceManager;
nsIDateTimeFormat* nsXULContentUtils::gFormat;

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
    if (gRefCnt++ == 0) {
        nsresult rv;
        rv = nsServiceManager::GetService(kRDFServiceCID,
                                          NS_GET_IID(nsIRDFService),
                                          (nsISupports**) &gRDF);
        if (NS_FAILED(rv)) return rv;

#define XUL_RESOURCE(ident, uri)            \
  PR_BEGIN_MACRO                            \
   rv = gRDF->GetResource((uri), &(ident)); \
   if (NS_FAILED(rv)) return rv;            \
  PR_END_MACRO

#define XUL_LITERAL(ident, val)                                   \
  PR_BEGIN_MACRO                                                  \
   rv = gRDF->GetLiteral(NS_LITERAL_STRING(val).get(), &(ident)); \
   if (NS_FAILED(rv)) return rv;                                  \
  PR_END_MACRO

#include "nsXULResourceList.h"
#undef XUL_RESOURCE
#undef XUL_LITERAL

        rv = nsComponentManager::CreateInstance(kNameSpaceManagerCID,
                                                nsnull,
                                                NS_GET_IID(nsINameSpaceManager),
                                                (void**) &gNameSpaceManager);
        if (NS_FAILED(rv)) return rv;

        rv = nsComponentManager::CreateInstance(kDateTimeFormatCID,
                                                nsnull,
                                                NS_GET_IID(nsIDateTimeFormat),
                                                (void**) &gFormat);

        if (NS_FAILED(rv)) return rv;
    }

    return gRefCnt;
}


nsresult
nsXULContentUtils::Finish()
{
    if (--gRefCnt == 0) {
        if (gRDF) {
            nsServiceManager::ReleaseService(kRDFServiceCID, gRDF);
            gRDF = nsnull;
        }

#define XUL_RESOURCE(ident, uri) NS_IF_RELEASE(ident)
#define XUL_LITERAL(ident, val) NS_IF_RELEASE(ident)
#include "nsXULResourceList.h"
#undef XUL_RESOURCE
#undef XUL_LITERAL

        NS_IF_RELEASE(gNameSpaceManager);
        NS_IF_RELEASE(gFormat);
    }

    return gRefCnt;
}


//------------------------------------------------------------------------
// nsIXULContentUtils methods

nsresult
nsXULContentUtils::FindChildByTag(nsIContent* aElement,
                                  PRInt32 aNameSpaceID,
                                  nsIAtom* aTag,
                                  nsIContent** aResult)
{
    nsresult rv;

    PRInt32 count;
    if (NS_FAILED(rv = aElement->ChildCount(count)))
        return rv;

    for (PRInt32 i = 0; i < count; ++i) {
        nsCOMPtr<nsIContent> kid;
        if (NS_FAILED(rv = aElement->ChildAt(i, *getter_AddRefs(kid))))
            return rv; // XXX fatal

        PRInt32 nameSpaceID;
        if (NS_FAILED(rv = kid->GetNameSpaceID(nameSpaceID)))
            return rv; // XXX fatal

        if (nameSpaceID != aNameSpaceID)
            continue; // wrong namespace

        nsCOMPtr<nsIAtom> kidTag;
        if (NS_FAILED(rv = kid->GetTag(*getter_AddRefs(kidTag))))
            return rv; // XXX fatal

        if (kidTag.get() != aTag)
            continue;

        *aResult = kid;
        NS_ADDREF(*aResult);
        return NS_OK;
    }

    *aResult = nsnull;
    return NS_RDF_NO_VALUE; // not found
}


nsresult
nsXULContentUtils::GetElementResource(nsIContent* aElement, nsIRDFResource** aResult)
{
    // Perform a reverse mapping from an element in the content model
    // to an RDF resource.
    nsresult rv;

    PRUnichar buf[128];
    nsAutoString id(CBufDescriptor(buf, PR_TRUE, sizeof(buf) / sizeof(PRUnichar), 0));

    rv = aElement->GetAttr(kNameSpaceID_None, nsXULAtoms::id, id);
    NS_ASSERTION(NS_SUCCEEDED(rv), "severe error retrieving attribute");
    if (NS_FAILED(rv)) return rv;

    if (rv != NS_CONTENT_ATTR_HAS_VALUE)
        return NS_ERROR_FAILURE;

    // Since the element will store its ID attribute as a document-relative value,
    // we may need to qualify it first...
    nsCOMPtr<nsIDocument> doc;
    rv = aElement->GetDocument(*getter_AddRefs(doc));
    if (NS_FAILED(rv)) return rv;

    NS_ASSERTION(doc != nsnull, "element is not in any document");
    if (! doc)
        return NS_ERROR_FAILURE;

    rv = nsXULContentUtils::MakeElementResource(doc, id, aResult);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


nsresult
nsXULContentUtils::GetElementRefResource(nsIContent* aElement, nsIRDFResource** aResult)
{
    // Perform a reverse mapping from an element in the content model
    // to an RDF resource. Check for a "ref" attribute first, then
    // fallback on an "id" attribute.
    nsresult rv;
    PRUnichar buf[128];
    nsAutoString uri(CBufDescriptor(buf, PR_TRUE, sizeof(buf) / sizeof(PRUnichar), 0));

    rv = aElement->GetAttr(kNameSpaceID_None, nsXULAtoms::ref, uri);
    NS_ASSERTION(NS_SUCCEEDED(rv), "severe error retrieving attribute");
    if (NS_FAILED(rv)) return rv;

    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
        // We'll use rdf_MakeAbsolute() to translate this to a URL.
        nsCOMPtr<nsIDocument> doc;
        rv = aElement->GetDocument(*getter_AddRefs(doc));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIURI> url;
        doc->GetDocumentURL(getter_AddRefs(url));
        NS_ASSERTION(url != nsnull, "element has no document");
        if (! url)
            return NS_ERROR_UNEXPECTED;

        // N.B. that if this fails (e.g., because necko doesn't grok
        // the protocol), uriStr will be untouched.
        NS_MakeAbsoluteURI(uri, uri, url);

        rv = gRDF->GetUnicodeResource(uri.get(), aResult);
    }
    else {
        rv = GetElementResource(aElement, aResult);
    }

    return rv;
}



/*
	Note: this routine is similiar, yet distinctly different from, nsBookmarksService::GetTextForNode
*/

nsresult
nsXULContentUtils::GetTextForNode(nsIRDFNode* aNode, nsAWritableString& aResult)
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
        PRInt64	value;
        rv = dateLiteral->GetValue(&value);
        if (NS_FAILED(rv)) return rv;

        nsAutoString str;
        rv = gFormat->FormatPRTime(nsnull /* nsILocale* locale */,
                                  kDateFormatShort,
                                  kTimeFormatSeconds,
                                  PRTime(value),
                                  str);
        aResult.Assign(str);

        if (NS_FAILED(rv)) return rv;

        return NS_OK;
    }

    nsCOMPtr<nsIRDFInt> intLiteral = do_QueryInterface(aNode);
    if (intLiteral) {
        PRInt32	value;
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
        aResult.Assign(NS_ConvertASCIItoUCS2(p));
        return NS_OK;
    }

    NS_ERROR("not a resource or a literal");
    return NS_ERROR_UNEXPECTED;
}

nsresult
nsXULContentUtils::MakeElementURI(nsIDocument* aDocument, const nsAReadableString& aElementID, nsCString& aURI)
{
    // Convert an element's ID to a URI that can be used to refer to
    // the element in the XUL graph.

    if (aElementID.FindChar(':') > 0) {
        // Assume it's absolute already. Use as is.
        aURI.Assign(NS_ConvertUCS2toUTF8(aElementID));
    }
    else {
        nsresult rv;

        nsCOMPtr<nsIURI> docURL;
        rv = aDocument->GetBaseURL(*getter_AddRefs(docURL));
        if (NS_FAILED(rv)) return rv;

        // XXX Urgh. This is so broken; I'd really just like to use
        // NS_MakeAbsolueURI(). Unfortunatly, doing that breaks
        // MakeElementID in some cases that I haven't yet been able to
        // figure out.
#define USE_BROKEN_RELATIVE_PARSING
#ifdef USE_BROKEN_RELATIVE_PARSING
        nsXPIDLCString spec;
        docURL->GetSpec(getter_Copies(spec));
        if (! spec)
            return NS_ERROR_FAILURE;

        aURI.Assign(spec);

        if (aElementID.First() != '#') {
            aURI.Append('#');
        }
        aURI.AppendWithConversion(aElementID);
#else
        nsXPIDLCString spec;
        rv = NS_MakeAbsoluteURI(nsCAutoString(aElementID), docURL, getter_Copies(spec));
        if (NS_SUCCEEDED(rv)) {
            aURI = spec;
        }
        else {
            NS_WARNING("MakeElementURI: NS_MakeAbsoluteURI failed");
            aURI = aElementID;
        }
#endif
    }

    return NS_OK;
}


nsresult
nsXULContentUtils::MakeElementResource(nsIDocument* aDocument, const nsAReadableString& aID, nsIRDFResource** aResult)
{
    nsresult rv;

    char buf[256];
    nsCAutoString uri(CBufDescriptor(buf, PR_TRUE, sizeof(buf), 0));
    rv = MakeElementURI(aDocument, aID, uri);
    if (NS_FAILED(rv)) return rv;

    rv = gRDF->GetResource(uri, aResult);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create resource");
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}



nsresult
nsXULContentUtils::MakeElementID(nsIDocument* aDocument, const nsAReadableString& aURI, nsAWritableString& aElementID)
{
    // Convert a URI into an element ID that can be accessed from the
    // DOM APIs.
    nsresult rv;

    nsCOMPtr<nsIURI> docURL;
    rv = aDocument->GetBaseURL(*getter_AddRefs(docURL));
    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString spec;
    docURL->GetSpec(getter_Copies(spec));
    if (! spec)
        return NS_ERROR_FAILURE;

    // XXX FIX ME to not do a copy
    nsAutoString str(aURI);
    if (str.Find(spec) == 0) {
#ifdef USE_BROKEN_RELATIVE_PARSING
        static const PRInt32 kFudge = 1;  // XXX assume '#'
#else
        static const PRInt32 kFudge = 0;
#endif
        PRInt32 len = PL_strlen(spec);
        aURI.Right(aElementID, aURI.Length() - (len + kFudge));
    }
    else {
        aElementID = aURI;
    }

    return NS_OK;
}

nsresult
nsXULContentUtils::GetResource(PRInt32 aNameSpaceID, nsIAtom* aAttribute, nsIRDFResource** aResult)
{
    // construct a fully-qualified URI from the namespace/tag pair.
    NS_PRECONDITION(aAttribute != nsnull, "null ptr");
    if (! aAttribute)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    nsAutoString attr;
    rv = aAttribute->ToString(attr);
    if (NS_FAILED(rv)) return rv;

    return GetResource(aNameSpaceID, attr, aResult);
}


nsresult
nsXULContentUtils::GetResource(PRInt32 aNameSpaceID, const nsAReadableString& aAttribute, nsIRDFResource** aResult)
{
    // construct a fully-qualified URI from the namespace/tag pair.

    // XXX should we allow nodes with no namespace???
    //NS_PRECONDITION(aNameSpaceID != kNameSpaceID_Unknown, "no namespace");
    //if (aNameSpaceID == kNameSpaceID_Unknown)
    //    return NS_ERROR_UNEXPECTED;

    nsresult rv;

    PRUnichar buf[256];
    nsAutoString uri(CBufDescriptor(buf, PR_TRUE, sizeof(buf) / sizeof(PRUnichar), 0));
    if (aNameSpaceID != kNameSpaceID_Unknown && aNameSpaceID != kNameSpaceID_None) {
        rv = gNameSpaceManager->GetNameSpaceURI(aNameSpaceID, uri);
        // XXX ignore failure; treat as "no namespace"
    }

    // XXX check to see if we need to insert a '/' or a '#'. Oy.
    if (uri.Length() > 0 && uri.Last() != '#' && uri.Last() != '/' && aAttribute.First() != '#')
        uri.AppendWithConversion('#');

    uri.Append(aAttribute);

    rv = gRDF->GetUnicodeResource(uri.get(), aResult);
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
    NS_PRECONDITION(aDocument != nsnull, "null ptr");
    if (! aDocument)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    nsCOMPtr<nsIDOMXULDocument> xuldoc = do_QueryInterface(aDocument);
    NS_ASSERTION(xuldoc != nsnull, "not a xul document");
    if (! xuldoc)
        return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIDOMXULCommandDispatcher> dispatcher;
    rv = xuldoc->GetCommandDispatcher(getter_AddRefs(dispatcher));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get dispatcher");
    if (NS_FAILED(rv)) return rv;

    NS_ASSERTION(dispatcher != nsnull, "no dispatcher");
    if (! dispatcher)
        return NS_ERROR_UNEXPECTED;

    nsAutoString events;
    rv = aElement->GetAttr(kNameSpaceID_None, nsXULAtoms::events, events);

    if (rv != NS_CONTENT_ATTR_HAS_VALUE)
        events.Assign(NS_LITERAL_STRING("*"));

    nsAutoString targets;
    rv = aElement->GetAttr(kNameSpaceID_None, nsXULAtoms::targets, targets);

    if (rv != NS_CONTENT_ATTR_HAS_VALUE)
        targets.Assign(NS_LITERAL_STRING("*"));

    nsCOMPtr<nsIDOMElement> domelement = do_QueryInterface(aElement);
    NS_ASSERTION(domelement != nsnull, "not a DOM element");
    if (! domelement)
        return NS_ERROR_UNEXPECTED;

    rv = dispatcher->AddCommandUpdater(domelement, events, targets);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


