/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */


/*

  A package of routines shared by the XUL content code.

 */


#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIRDFNode.h"
#include "nsINameSpace.h"
#include "nsINameSpaceManager.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsITextContent.h"
#include "nsIURL.h"
#include "nsIXMLContent.h"
#include "nsIXULContentUtils.h"
#include "nsLayoutCID.h"
#include "nsNeckoUtil.h"
#include "nsRDFCID.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "prlog.h"
#include "prtime.h"
#include "rdf.h"
#include "rdfutil.h"

#include "nsILocale.h"
#include "nsLocaleCID.h"
#include "nsILocaleFactory.h"

#include "nsIDateTimeFormat.h"
#include "nsDateTimeFormatCID.h"
#include "nsIScriptableDateFormat.h"

static NS_DEFINE_IID(kIContentIID,     NS_ICONTENT_IID);
static NS_DEFINE_IID(kIRDFResourceIID, NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,  NS_IRDFLITERAL_IID);
static NS_DEFINE_IID(kIRDFIntIID,      NS_IRDFINT_IID);
static NS_DEFINE_IID(kIRDFDateIID,     NS_IRDFDATE_IID);
static NS_DEFINE_IID(kITextContentIID, NS_ITEXT_CONTENT_IID); // XXX grr...
static NS_DEFINE_CID(kTextNodeCID,     NS_TEXTNODE_CID);
static NS_DEFINE_CID(kRDFServiceCID,   NS_RDFSERVICE_CID);

static NS_DEFINE_CID(kLocaleFactoryCID, NS_LOCALEFACTORY_CID);
static NS_DEFINE_IID(kILocaleFactoryIID, NS_ILOCALEFACTORY_IID);
static NS_DEFINE_CID(kLocaleCID, NS_LOCALE_CID);
static NS_DEFINE_IID(kILocaleIID, NS_ILOCALE_IID);
static NS_DEFINE_CID(kNameSpaceManagerCID, NS_NAMESPACEMANAGER_CID);

static NS_DEFINE_CID(kDateTimeFormatCID, NS_DATETIMEFORMAT_CID);
static NS_DEFINE_CID(kDateTimeFormatIID, NS_IDATETIMEFORMAT_IID);

//------------------------------------------------------------------------

class nsXULContentUtils : public nsIXULContentUtils
{
protected:
    nsXULContentUtils();
    nsresult Init();
    virtual ~nsXULContentUtils();

    friend NS_IMETHODIMP
    NS_NewXULContentUtils(nsISupports* aOuter, const nsIID& aIID, void** aResult);

    static nsrefcnt gRefCnt;
    static nsIRDFService* gRDF;
    static nsINameSpaceManager* gNameSpaceManager;
    static nsIDateTimeFormat* gFormat;

public:
    // nsISupports methods
    NS_DECL_ISUPPORTS

    // nsIXULContentUtils methods
    NS_IMETHOD
    AttachTextNode(nsIContent* parent, nsIRDFNode* value);
    
    NS_IMETHOD
    FindChildByTag(nsIContent *aElement,
                   PRInt32 aNameSpaceID,
                   nsIAtom* aTag,
                   nsIContent **aResult);

    NS_IMETHOD
    FindChildByResource(nsIContent* aElement,
                        nsIRDFResource* aResource,
                        nsIContent** aResult);

    NS_IMETHOD
    GetElementResource(nsIContent* aElement, nsIRDFResource** aResult);

    NS_IMETHOD
    GetElementRefResource(nsIContent* aElement, nsIRDFResource** aResult);

    NS_IMETHOD
    GetTextForNode(nsIRDFNode* aNode, nsString& aResult);

    NS_IMETHOD
    GetElementLogString(nsIContent* aElement, nsString& aResult);

    NS_IMETHOD
    GetAttributeLogString(nsIContent* aElement, PRInt32 aNameSpaceID, nsIAtom* aTag, nsString& aResult);

    NS_IMETHOD
    MakeElementURI(nsIDocument* aDocument, const nsString& aElementID, nsCString& aURI);

    NS_IMETHOD
    MakeElementResource(nsIDocument* aDocument, const nsString& aElementID, nsIRDFResource** aResult);

    NS_IMETHOD
    MakeElementID(nsIDocument* aDocument, const nsString& aURI, nsString& aElementID);

    NS_IMETHOD_(PRBool)
    IsContainedBy(nsIContent* aElement, nsIContent* aContainer);

    NS_IMETHOD
    GetResource(PRInt32 aNameSpaceID, nsIAtom* aAttribute, nsIRDFResource** aResult);

    NS_IMETHOD
    GetResource(PRInt32 aNameSpaceID, const nsString& aAttribute, nsIRDFResource** aResult);

};

nsrefcnt nsXULContentUtils::gRefCnt;
nsIRDFService* nsXULContentUtils::gRDF;
nsINameSpaceManager* nsXULContentUtils::gNameSpaceManager;
nsIDateTimeFormat* nsXULContentUtils::gFormat;

//------------------------------------------------------------------------
// Constructors n' stuff

nsXULContentUtils::nsXULContentUtils()
{
    NS_INIT_REFCNT();
}

nsresult
nsXULContentUtils::Init()
{
    if (gRefCnt++ == 0) {
        nsresult rv;
        rv = nsServiceManager::GetService(kRDFServiceCID,
                                          nsCOMTypeInfo<nsIRDFService>::GetIID(),
                                          (nsISupports**) &gRDF);
        if (NS_FAILED(rv)) return rv;

        rv = nsComponentManager::CreateInstance(kNameSpaceManagerCID,
                                                nsnull,
                                                nsCOMTypeInfo<nsINameSpaceManager>::GetIID(),
                                                (void**) &gNameSpaceManager);
        if (NS_FAILED(rv)) return rv;

        rv = nsComponentManager::CreateInstance(kDateTimeFormatCID,
                                                nsnull,
                                                nsCOMTypeInfo<nsIDateTimeFormat>::GetIID(),
                                                (void**) &gFormat);

        if (NS_FAILED(rv)) return rv;
    }
    return NS_OK;
}


nsXULContentUtils::~nsXULContentUtils()
{
    if (--gRefCnt == 0) {
        if (gRDF) {
            nsServiceManager::ReleaseService(kRDFServiceCID, gRDF);
            gRDF = nsnull;
        }

        NS_IF_RELEASE(gNameSpaceManager);
    }
}

NS_IMETHODIMP
NS_NewXULContentUtils(nsISupports* aOuter, const nsIID& aIID, void** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aOuter == nsnull, "no aggregation");
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsXULContentUtils* result = new nsXULContentUtils();
    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv;
    rv = result->Init();
    if (NS_FAILED(rv)) {
        delete result;
        return rv;
    }

    NS_ADDREF(result);
    rv = result->QueryInterface(aIID, aResult);
    NS_RELEASE(result);

    return rv;
}



//------------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ISUPPORTS(nsXULContentUtils, nsCOMTypeInfo<nsIXULContentUtils>::GetIID());

//------------------------------------------------------------------------
// nsIXULContentUtils methods

NS_IMETHODIMP
nsXULContentUtils::AttachTextNode(nsIContent* parent, nsIRDFNode* value)
{
    nsresult rv;

    nsAutoString s;
    rv = GetTextForNode(value, s);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsITextContent> text;
    rv = nsComponentManager::CreateInstance(kTextNodeCID,
                                            nsnull,
                                            nsITextContent::GetIID(),
                                            getter_AddRefs(text));
    if (NS_FAILED(rv)) return rv;


    rv = text->SetText(s.GetUnicode(), s.Length(), PR_FALSE);
    if (NS_FAILED(rv)) return rv;

    // hook it up to the child
    rv = parent->AppendChildTo(nsCOMPtr<nsIContent>( do_QueryInterface(text) ), PR_FALSE);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


NS_IMETHODIMP
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

    return NS_RDF_NO_VALUE; // not found
}


NS_IMETHODIMP
nsXULContentUtils::FindChildByResource(nsIContent* aElement,
                                       nsIRDFResource* aResource,
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

        // Now get the resource ID from the RDF:ID attribute. We do it
        // via the content model, because you're never sure who
        // might've added this stuff in...
        nsCOMPtr<nsIRDFResource> resource;
        rv = GetElementResource(kid, getter_AddRefs(resource));
        if (NS_FAILED(rv)) continue;

        if (resource.get() != aResource)
            continue; // not the resource we want

        // Fount it!
        *aResult = kid;
        NS_ADDREF(*aResult);
        return NS_OK;
    }

    return NS_RDF_NO_VALUE; // not found
}


NS_IMETHODIMP
nsXULContentUtils::GetElementResource(nsIContent* aElement, nsIRDFResource** aResult)
{
    // Perform a reverse mapping from an element in the content model
    // to an RDF resource.
    nsresult rv;
    nsAutoString id;

    nsCOMPtr<nsIAtom> kIdAtom( dont_AddRef(NS_NewAtom("id")) );
    rv = aElement->GetAttribute(kNameSpaceID_None, kIdAtom, id);
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


NS_IMETHODIMP
nsXULContentUtils::GetElementRefResource(nsIContent* aElement, nsIRDFResource** aResult)
{
    // Perform a reverse mapping from an element in the content model
    // to an RDF resource. Check for a "ref" attribute first, then
    // fallback on an "id" attribute.
    nsresult rv;
    nsAutoString uri;

    nsCOMPtr<nsIAtom> kIdAtom( dont_AddRef(NS_NewAtom("ref")) );
    rv = aElement->GetAttribute(kNameSpaceID_None, kIdAtom, uri);
    NS_ASSERTION(NS_SUCCEEDED(rv), "severe error retrieving attribute");
    if (NS_FAILED(rv)) return rv;

    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
        // We'll use rdf_MakeAbsolute() to translate this to a URL.
        nsCOMPtr<nsIDocument> doc;
        rv = aElement->GetDocument(*getter_AddRefs(doc));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIURI> url = dont_AddRef( doc->GetDocumentURL() );
        NS_ASSERTION(url != nsnull, "element has no document");
        if (! url)
            return NS_ERROR_UNEXPECTED;

        rv = rdf_MakeAbsoluteURI(url, uri);
        if (NS_FAILED(rv)) return rv;

        rv = gRDF->GetUnicodeResource(uri.GetUnicode(), aResult);
    }
    else {
        rv = GetElementResource(aElement, aResult);
    }

    return rv;
}



/*
	Note: this routine is similiar, yet distinctly different from, nsBookmarksService::GetTextForNode
*/

NS_IMETHODIMP
nsXULContentUtils::GetTextForNode(nsIRDFNode* aNode, nsString& aResult)
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
        PRInt64	tm;
        rv = dateLiteral->GetValue(&tm);
        if (NS_FAILED(rv)) return rv;

        rv = gFormat->FormatPRTime(nsnull /* nsILocale* locale */,
                                  kDateFormatShort,
                                  kTimeFormatSeconds,
                                  PRTime(tm),
                                  aResult);

        if (NS_FAILED(rv)) return rv;

        return NS_OK;
    }

    nsCOMPtr<nsIRDFInt> intLiteral = do_QueryInterface(aNode);
    if (intLiteral) {
        PRInt32	value;
        rv = intLiteral->GetValue(&value);
        if (NS_FAILED(rv)) return rv;

        aResult.Truncate();
        aResult.Append(value, 10);
        return NS_OK;
    }


    nsCOMPtr<nsIRDFResource> resource = do_QueryInterface(aNode);
    if (resource) {
        const char* p;
        rv = resource->GetValueConst(&p);
        if (NS_FAILED(rv)) return rv;
        aResult = p;
        return NS_OK;
    }

    NS_ERROR("not a resource or a literal");
    return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsXULContentUtils::GetElementLogString(nsIContent* aElement, nsString& aResult)
{
    nsresult rv;

    aResult = '<';

    nsCOMPtr<nsINameSpace> ns;

    PRInt32 elementNameSpaceID;
    rv = aElement->GetNameSpaceID(elementNameSpaceID);
    if (NS_FAILED(rv)) return rv;

    if (kNameSpaceID_HTML == elementNameSpaceID) {
        aResult.Append("html:");
    }
    else {
        nsCOMPtr<nsIXMLContent> xml( do_QueryInterface(aElement) );
        NS_ASSERTION(xml != nsnull, "not an XML or HTML element");
        if (! xml) return NS_ERROR_UNEXPECTED;

        rv = xml->GetContainingNameSpace(*getter_AddRefs(ns));
        if (NS_FAILED(rv)) return rv;
        
        nsCOMPtr<nsIAtom> prefix;
        rv = ns->FindNameSpacePrefix(elementNameSpaceID, *getter_AddRefs(prefix));
        if (NS_SUCCEEDED(rv) && (prefix != nsnull)) {
            nsAutoString prefixStr;
            prefix->ToString(prefixStr);
            if (prefixStr.Length()) {
                const PRUnichar *unicodeString;
                prefix->GetUnicode(&unicodeString);
                aResult.Append(unicodeString);
                aResult.Append(':');
            }
        }
    }

    nsCOMPtr<nsIAtom> tag;
    rv = aElement->GetTag(*getter_AddRefs(tag));
    if (NS_FAILED(rv)) return rv;

    const PRUnichar *unicodeString;
    tag->GetUnicode(&unicodeString);
    aResult.Append(unicodeString);

    PRInt32 count;
    rv = aElement->GetAttributeCount(count);
    if (NS_FAILED(rv)) return rv;

    for (PRInt32 i = 0; i < count; ++i) {
        aResult.Append(' ');

        PRInt32 nameSpaceID;
        nsCOMPtr<nsIAtom> name;
        rv = aElement->GetAttributeNameAt(i, nameSpaceID, *getter_AddRefs(name));
        if (NS_FAILED(rv)) return rv;

        nsAutoString attr;
        nsXULContentUtils::GetAttributeLogString(aElement, nameSpaceID, name, attr);

        aResult.Append(attr);
        aResult.Append("=\"");

        nsAutoString value;
        rv = aElement->GetAttribute(nameSpaceID, name, value);
        if (NS_FAILED(rv)) return rv;

        aResult.Append(value);
        aResult.Append("\"");
    }

    aResult.Append('>');
    return NS_OK;
}


NS_IMETHODIMP
nsXULContentUtils::GetAttributeLogString(nsIContent* aElement, PRInt32 aNameSpaceID, nsIAtom* aTag, nsString& aResult)
{
    nsresult rv;

    PRInt32 elementNameSpaceID;
    rv = aElement->GetNameSpaceID(elementNameSpaceID);
    if (NS_FAILED(rv)) return rv;

    if ((kNameSpaceID_HTML == elementNameSpaceID) ||
        (kNameSpaceID_None == aNameSpaceID)) {
        aResult.Truncate();
    }
    else {
        // we may have a namespace prefix on the attribute
        nsCOMPtr<nsIXMLContent> xml( do_QueryInterface(aElement) );
        NS_ASSERTION(xml != nsnull, "not an XML or HTML element");
        if (! xml) return NS_ERROR_UNEXPECTED;

        nsCOMPtr<nsINameSpace> ns;
        rv = xml->GetContainingNameSpace(*getter_AddRefs(ns));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIAtom> prefix;
        rv = ns->FindNameSpacePrefix(aNameSpaceID, *getter_AddRefs(prefix));
        if (NS_SUCCEEDED(rv) && (prefix != nsnull)) {
            nsAutoString prefixStr;
            prefix->ToString(prefixStr);
            if (prefixStr.Length()) {
                const PRUnichar *unicodeString;
                prefix->GetUnicode(&unicodeString);
                aResult.Append(unicodeString);
                aResult.Append(':');
            }
        }
    }

    const PRUnichar *unicodeString;
    aTag->GetUnicode(&unicodeString);
    aResult.Append(unicodeString);
    return NS_OK;
}


NS_IMETHODIMP
nsXULContentUtils::MakeElementURI(nsIDocument* aDocument, const nsString& aElementID, nsCString& aURI)
{
    // Convert an element's ID to a URI that can be used to refer to
    // the element in the XUL graph.

    if (aElementID.FindChar(':') > 0) {
        // Assume it's absolute already. Use as is.
        aURI = aElementID;
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

        aURI = spec;

        if (aElementID.First() != '#') {
            aURI.Append('#');
        }
        aURI.Append(nsCAutoString(aElementID));
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


NS_IMETHODIMP
nsXULContentUtils::MakeElementResource(nsIDocument* aDocument, const nsString& aID, nsIRDFResource** aResult)
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



NS_IMETHODIMP
nsXULContentUtils::MakeElementID(nsIDocument* aDocument, const nsString& aURI, nsString& aElementID)
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

    if (aURI.Find(spec) == 0) {
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

NS_IMETHODIMP_(PRBool)
nsXULContentUtils::IsContainedBy(nsIContent* aElement, nsIContent* aContainer)
{
    nsCOMPtr<nsIContent> element( dont_QueryInterface(aElement) );
    while (element) {
        nsresult rv;

        nsCOMPtr<nsIContent> parent;
        rv = element->GetParent(*getter_AddRefs(parent));
        if (NS_FAILED(rv)) return PR_FALSE;

        if (parent.get() == aContainer)
            return PR_TRUE;

        element = parent;
    }

    return PR_FALSE;
}


NS_IMETHODIMP
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


NS_IMETHODIMP
nsXULContentUtils::GetResource(PRInt32 aNameSpaceID, const nsString& aAttribute, nsIRDFResource** aResult)
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
        uri.Append('#');

    uri.Append(aAttribute);

    rv = gRDF->GetUnicodeResource(uri.GetUnicode(), aResult);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource");
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}
