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
#include "nsIDOMMouseListener.h"
#include "nsIDOMMouseMotionListener.h"
#include "nsIDOMLoadListener.h"
#include "nsIDOMFocusListener.h"
#include "nsIDOMPaintListener.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMFormListener.h"
#include "nsIDOMMenuListener.h"
#include "nsIDOMScrollListener.h"
#include "nsIDOMDragListener.h"
#include "nsIRDFNode.h"
#include "nsINameSpace.h"
#include "nsINameSpaceManager.h"
#include "nsIPref.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsITextContent.h"
#include "nsIURL.h"
#include "nsIXMLContent.h"
#include "nsIXULContentUtils.h"
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
#include "rdfutil.h"

#include "nsIDateTimeFormat.h"
#include "nsDateTimeFormatCID.h"
#include "nsIScriptableDateFormat.h"


static NS_DEFINE_CID(kDateTimeFormatCID,    NS_DATETIMEFORMAT_CID);
static NS_DEFINE_CID(kDateTimeFormatIID,    NS_IDATETIMEFORMAT_IID);
static NS_DEFINE_CID(kNameSpaceManagerCID,  NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_CID(kRDFServiceCID,        NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kTextNodeCID,          NS_TEXTNODE_CID);
static NS_DEFINE_CID(kXULPrototypeCacheCID, NS_XULPROTOTYPECACHE_CID);

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

    struct EventHandlerMapEntry {
        const char*  mAttributeName;
        nsIAtom*     mAttributeAtom;
        const nsIID* mHandlerIID;
    };

    static EventHandlerMapEntry kEventHandlerMap[];

    static PRBool gDisableXULCache;

    static int PR_CALLBACK
    DisableXULCacheChangedCallback(const char* aPrefName, void* aClosure);

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
    GetTextForNode(nsIRDFNode* aNode, nsAWritableString& aResult);

    NS_IMETHOD
    GetElementLogString(nsIContent* aElement, nsAWritableString& aResult);

    NS_IMETHOD
    GetAttributeLogString(nsIContent* aElement, PRInt32 aNameSpaceID, nsIAtom* aTag, nsAWritableString& aResult);

    NS_IMETHOD
    MakeElementURI(nsIDocument* aDocument, const nsAReadableString& aElementID, nsCString& aURI);

    NS_IMETHOD
    MakeElementResource(nsIDocument* aDocument, const nsAReadableString& aElementID, nsIRDFResource** aResult);

    NS_IMETHOD
    MakeElementID(nsIDocument* aDocument, const nsAReadableString& aURI, nsAWritableString& aElementID);

    NS_IMETHOD_(PRBool)
    IsContainedBy(nsIContent* aElement, nsIContent* aContainer);

    NS_IMETHOD
    GetResource(PRInt32 aNameSpaceID, nsIAtom* aAttribute, nsIRDFResource** aResult);

    NS_IMETHOD
    GetResource(PRInt32 aNameSpaceID, const nsAReadableString& aAttribute, nsIRDFResource** aResult);

    NS_IMETHOD
    SetCommandUpdater(nsIDocument* aDocument, nsIContent* aElement);

    NS_IMETHOD
    GetEventHandlerIID(nsIAtom* aName, nsIID* aIID, PRBool* aFound);

    NS_IMETHOD_(PRBool)
    UseXULCache();
};

nsrefcnt nsXULContentUtils::gRefCnt;
nsIRDFService* nsXULContentUtils::gRDF;
nsINameSpaceManager* nsXULContentUtils::gNameSpaceManager;
nsIDateTimeFormat* nsXULContentUtils::gFormat;

nsXULContentUtils::EventHandlerMapEntry
nsXULContentUtils::kEventHandlerMap[] = {
    { "onclick",         nsnull, &NS_GET_IID(nsIDOMMouseListener)       },
    { "ondblclick",      nsnull, &NS_GET_IID(nsIDOMMouseListener)       },
    { "onmousedown",     nsnull, &NS_GET_IID(nsIDOMMouseListener)       },
    { "onmouseup",       nsnull, &NS_GET_IID(nsIDOMMouseListener)       },
    { "onmouseover",     nsnull, &NS_GET_IID(nsIDOMMouseListener)       },
    { "onmouseout",      nsnull, &NS_GET_IID(nsIDOMMouseListener)       },

    { "onmousemove",     nsnull, &NS_GET_IID(nsIDOMMouseMotionListener) },

    { "onkeydown",       nsnull, &NS_GET_IID(nsIDOMKeyListener)         },
    { "onkeyup",         nsnull, &NS_GET_IID(nsIDOMKeyListener)         },
    { "onkeypress",      nsnull, &NS_GET_IID(nsIDOMKeyListener)         },

    { "onload",          nsnull, &NS_GET_IID(nsIDOMLoadListener)        },
    { "onunload",        nsnull, &NS_GET_IID(nsIDOMLoadListener)        },
    { "onabort",         nsnull, &NS_GET_IID(nsIDOMLoadListener)        },
    { "onerror",         nsnull, &NS_GET_IID(nsIDOMLoadListener)        },

    { "oncreate",        nsnull, &NS_GET_IID(nsIDOMMenuListener)        },
    { "onclose",         nsnull, &NS_GET_IID(nsIDOMMenuListener)        },
    { "ondestroy",       nsnull, &NS_GET_IID(nsIDOMMenuListener)        },
    { "oncommand",       nsnull, &NS_GET_IID(nsIDOMMenuListener)        },
    { "onbroadcast",     nsnull, &NS_GET_IID(nsIDOMMenuListener)        },
    { "oncommandupdate", nsnull, &NS_GET_IID(nsIDOMMenuListener)        },

    { "onoverflow",       nsnull, &NS_GET_IID(nsIDOMScrollListener)     },
    { "onunderflow",      nsnull, &NS_GET_IID(nsIDOMScrollListener)     },
    { "onoverflowchanged",nsnull, &NS_GET_IID(nsIDOMScrollListener)     },

    { "onfocus",         nsnull, &NS_GET_IID(nsIDOMFocusListener)       },
    { "onblur",          nsnull, &NS_GET_IID(nsIDOMFocusListener)       },

    { "onsubmit",        nsnull, &NS_GET_IID(nsIDOMFormListener)        },
    { "onreset",         nsnull, &NS_GET_IID(nsIDOMFormListener)        },
    { "onchange",        nsnull, &NS_GET_IID(nsIDOMFormListener)        },
    { "onselect",        nsnull, &NS_GET_IID(nsIDOMFormListener)        },
    { "oninput",         nsnull, &NS_GET_IID(nsIDOMFormListener)        },

    { "onpaint",         nsnull, &NS_GET_IID(nsIDOMPaintListener)       },
    
    { "ondragenter",     nsnull, &NS_GET_IID(nsIDOMDragListener)        },
    { "ondragover",      nsnull, &NS_GET_IID(nsIDOMDragListener)        },
    { "ondragexit",      nsnull, &NS_GET_IID(nsIDOMDragListener)        },
    { "ondragdrop",      nsnull, &NS_GET_IID(nsIDOMDragListener)        },
    { "ondraggesture",   nsnull, &NS_GET_IID(nsIDOMDragListener)        },

    { nsnull,            nsnull, nsnull                                 }
};


// Enabled by default. Must be over-ridden to disable
PRBool nsXULContentUtils::gDisableXULCache = PR_FALSE;


static const char kDisableXULCachePref[] = "nglayout.debug.disable_xul_cache";

//------------------------------------------------------------------------
// Constructors n' stuff
//

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
                                          NS_GET_IID(nsIRDFService),
                                          (nsISupports**) &gRDF);
        if (NS_FAILED(rv)) return rv;

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

        EventHandlerMapEntry* entry = kEventHandlerMap;
        while (entry->mAttributeName) {
            entry->mAttributeAtom = NS_NewAtom(entry->mAttributeName);
            ++entry;
        }

        NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_CONTRACTID, &rv);
        if (NS_SUCCEEDED(rv)) {
            // XXX Ignore return values.
            prefs->GetBoolPref(kDisableXULCachePref, &gDisableXULCache);
            prefs->RegisterCallback(kDisableXULCachePref, DisableXULCacheChangedCallback, nsnull);
        }

        nsXULAtoms::AddRef();
    }
    return NS_OK;
}


nsXULContentUtils::~nsXULContentUtils()
{
#ifdef DEBUG_REFS
    --gInstanceCount;
    fprintf(stdout, "%d - RDF: nsXULContentUtils\n", gInstanceCount);
#endif

    if (--gRefCnt == 0) {
        if (gRDF) {
            nsServiceManager::ReleaseService(kRDFServiceCID, gRDF);
            gRDF = nsnull;
        }

        NS_IF_RELEASE(gNameSpaceManager);
        NS_IF_RELEASE(gFormat);

        EventHandlerMapEntry* entry = kEventHandlerMap;
        while (entry->mAttributeName) {
            NS_IF_RELEASE(entry->mAttributeAtom);
            ++entry;
        }

        nsXULAtoms::Release();
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

NS_IMPL_ISUPPORTS(nsXULContentUtils, NS_GET_IID(nsIXULContentUtils));

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
                                            NS_GET_IID(nsITextContent),
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

    *aResult = nsnull;
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

    PRUnichar buf[128];
    nsAutoString id(CBufDescriptor(buf, PR_TRUE, sizeof(buf) / sizeof(PRUnichar), 0));

    rv = aElement->GetAttribute(kNameSpaceID_None, nsXULAtoms::id, id);
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
    PRUnichar buf[128];
    nsAutoString uri(CBufDescriptor(buf, PR_TRUE, sizeof(buf) / sizeof(PRUnichar), 0));

    rv = aElement->GetAttribute(kNameSpaceID_None, nsXULAtoms::ref, uri);
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

    		nsCAutoString uriStr;
        uriStr.Assign(NS_ConvertUCS2toUTF8(uri));
        rv = rdf_MakeAbsoluteURI(url, uriStr);
        if (NS_FAILED(rv)) return rv;

        rv = gRDF->GetResource(uriStr.GetBuffer(), aResult);
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

NS_IMETHODIMP
nsXULContentUtils::GetElementLogString(nsIContent* aElement, nsAWritableString& aResult)
{
    nsresult rv;

    aResult.Assign(PRUnichar('<'));

    nsCOMPtr<nsINameSpace> ns;

    PRInt32 elementNameSpaceID;
    rv = aElement->GetNameSpaceID(elementNameSpaceID);
    if (NS_FAILED(rv)) return rv;

    if (kNameSpaceID_HTML == elementNameSpaceID) {
        aResult.Append(NS_LITERAL_STRING("html:"));
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
                aResult.Append(NS_LITERAL_STRING(":"));
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
        aResult.Append(NS_LITERAL_STRING(" "));

        PRInt32 nameSpaceID;
        nsCOMPtr<nsIAtom> name, prefix;
        rv = aElement->GetAttributeNameAt(i, nameSpaceID, *getter_AddRefs(name), *getter_AddRefs(prefix));
        if (NS_FAILED(rv)) return rv;

        nsAutoString attr;
        nsXULContentUtils::GetAttributeLogString(aElement, nameSpaceID, name, attr);

        aResult.Append(attr);
        aResult.Append(NS_LITERAL_STRING("=\""));

        nsAutoString value;
        rv = aElement->GetAttribute(nameSpaceID, name, value);
        if (NS_FAILED(rv)) return rv;

        aResult.Append(value);
        aResult.Append(NS_LITERAL_STRING("\""));
    }

    aResult.Append(NS_LITERAL_STRING(">"));
    return NS_OK;
}


NS_IMETHODIMP
nsXULContentUtils::GetAttributeLogString(nsIContent* aElement, PRInt32 aNameSpaceID, nsIAtom* aTag, nsAWritableString& aResult)
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
                aResult.Append(NS_LITERAL_STRING(":"));
            }
        }
    }

    const PRUnichar *unicodeString;
    aTag->GetUnicode(&unicodeString);
    aResult.Append(unicodeString);
    return NS_OK;
}


NS_IMETHODIMP
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


NS_IMETHODIMP
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



NS_IMETHODIMP
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

    rv = gRDF->GetUnicodeResource(uri.GetUnicode(), aResult);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource");
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


NS_IMETHODIMP
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
    rv = aElement->GetAttribute(kNameSpaceID_None, nsXULAtoms::events, events);

    if (rv != NS_CONTENT_ATTR_HAS_VALUE)
        events.Assign(NS_LITERAL_STRING("*"));

    nsAutoString targets;
    rv = aElement->GetAttribute(kNameSpaceID_None, nsXULAtoms::targets, targets);

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

NS_IMETHODIMP
nsXULContentUtils::GetEventHandlerIID(nsIAtom* aName, nsIID* aIID, PRBool* aFound)
{
    *aFound = PR_FALSE;

    EventHandlerMapEntry* entry = kEventHandlerMap;
    while (entry->mAttributeAtom) {
        if (entry->mAttributeAtom == aName) {
            *aIID = *entry->mHandlerIID;
            *aFound = PR_TRUE;
            break;
        }
        ++entry;
    }

    return NS_OK;
}


NS_IMETHODIMP_(PRBool)
nsXULContentUtils::UseXULCache()
{
    return !gDisableXULCache;
}


//----------------------------------------------------------------------

int
nsXULContentUtils::DisableXULCacheChangedCallback(const char* aPref, void* aClosure)
{
    nsresult rv;

    NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        prefs->GetBoolPref(kDisableXULCachePref, &gDisableXULCache);
    }

    // Flush the cache, regardless
    NS_WITH_SERVICE(nsIXULPrototypeCache, cache, kXULPrototypeCacheCID, &rv);
    if (NS_SUCCEEDED(rv)) {
        cache->Flush();
    }

    return 0;
}
