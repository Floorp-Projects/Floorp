/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Vladimir Vukicevic
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsBookmarksService.h"
#include "nsArrayEnumerator.h"
#include "nsArray.h"
#include "nsIDOMWindow.h"
#include "nsIObserverService.h"
#include "nsIRDFContainer.h"
#include "nsIRDFContainerUtils.h"
#include "nsIRDFService.h"
#include "nsIRDFXMLSerializer.h"
#include "nsIRDFXMLSource.h"
#include "nsIRDFXMLParser.h"
#include "nsRDFCID.h"
#include "nsISupportsPrimitives.h"
#include "rdf.h"
#include "nsUnicharUtils.h"

#include "nsIURL.h"
#include "nsIInputStream.h"
#include "nsNetUtil.h"

#include "nsIDOMParser.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMNodeList.h"

/* These are defined in nsBookmarksService.cpp */
extern nsIRDFResource       *kRDF_type;
extern nsIRDFResource       *kRDF_nextVal;

extern nsIRDFResource       *kNC_FeedURL;
extern nsIRDFResource       *kNC_LivemarkLock;
extern nsIRDFResource       *kNC_LivemarkExpiration;

extern nsIRDFResource       *kRSS09_channel;
extern nsIRDFResource       *kRSS09_item;
extern nsIRDFResource       *kRSS09_title;
extern nsIRDFResource       *kRSS09_link;

extern nsIRDFResource       *kRSS10_channel;
extern nsIRDFResource       *kRSS10_items;
extern nsIRDFResource       *kRSS10_title;
extern nsIRDFResource       *kRSS10_link;

extern nsIRDFLiteral        *kTrueLiteral;

extern nsIRDFService        *gRDF;
extern nsIRDFContainerUtils *gRDFC;

static NS_DEFINE_CID(kRDFContainerCID,            NS_RDFCONTAINER_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID,   NS_RDFINMEMORYDATASOURCE_CID);

nsresult nsBMSVCClearSeqContainer (nsIRDFDataSource* aDataSource, nsIRDFResource* aResource);
nsresult nsBMSVCUnmakeSeq (nsIRDFDataSource* aDataSource, nsIRDFResource* aResource);

////////////////////////////////////////////////////////////////////////
// nsFeedLoadListener
//
// An nsIStreamListener implementation that UpdateLivemarkChildren uses
// to aysnchronously fetch and update a livemark's child entries.
//
// This could potentially be pulled out to its own file, or become
// a nested class within the bookmarks service.
//

class nsFeedLoadListener : public nsIStreamListener
{
public:
    nsFeedLoadListener(nsBookmarksService *aBMSVC,
                       nsIRDFDataSource *aInnerBMDataSource,
                       nsIURI *aURI,
                       nsIRDFResource *aLivemarkResource)
        : mBMSVC(aBMSVC), mInnerBMDataSource(aInnerBMDataSource),
          mURI(aURI), mResource(aLivemarkResource), mAborted(PR_FALSE)
    {
        NS_IF_ADDREF(mBMSVC);
    }

    virtual ~nsFeedLoadListener() {
        NS_IF_RELEASE(mBMSVC);
    }

    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIREQUESTOBSERVER

    void Abort () { mAborted = PR_TRUE; }

protected:
    static NS_METHOD StreamReaderCallback(nsIInputStream *in,
                                          void *closure,
                                          const char *fromRawSegment,
                                          PRUint32 toOffset,
                                          PRUint32 count,
                                          PRUint32 *writeCount);

    NS_METHOD TryParseAsRDF();
    NS_METHOD TryParseAsSimpleRSS();
    NS_METHOD SetResourceTTL(PRInt32 ttl);

    // helpers
    NS_METHOD HandleRDFItem (nsIRDFDataSource *aDS, nsIRDFResource *itemResource,
                             nsIRDFResource *aLinkResource, nsIRDFResource *aTitleResource);
    NS_METHOD FindTextNode (nsIDOMNode *aParentNode, nsIDOMNode **aTextNode);

    nsBookmarksService *mBMSVC;
    nsCOMPtr<nsIRDFDataSource> mInnerBMDataSource;
    nsCOMPtr<nsIURI> mURI;
    nsCOMPtr<nsIRDFResource> mResource;
    nsCOMPtr<nsIOutputStream> mCacheStream;
    PRBool mAborted;
    nsCString mBody;
    nsCOMPtr<nsIRDFContainer> mLivemarkContainer;
};

NS_IMPL_ISUPPORTS2(nsFeedLoadListener, nsIStreamListener, nsIRequestObserver)

NS_IMETHODIMP
nsFeedLoadListener::OnStartRequest(nsIRequest *aResult, nsISupports *ctxt)
{
    if (mAborted)
        return NS_ERROR_UNEXPECTED;

    mBody.Truncate();
    return NS_OK;
}

NS_METHOD
nsFeedLoadListener::StreamReaderCallback(nsIInputStream *aInputStream,
                                         void *aClosure,
                                         const char *aRawSegment,
                                         PRUint32 aToOffset,
                                         PRUint32 aCount,
                                         PRUint32 *aWriteCount)
{
    nsFeedLoadListener *rll = (nsFeedLoadListener *) aClosure;

    rll->mBody.Append(aRawSegment, aCount);
    *aWriteCount = aCount;

    return NS_OK;
}

NS_IMETHODIMP
nsFeedLoadListener::OnDataAvailable(nsIRequest *aRequest,
                                    nsISupports *aContext,
                                    nsIInputStream *aInputStream,
                                    PRUint32 aSourceOffset,
                                    PRUint32 aCount)
{
    PRUint32 totalRead;
    return aInputStream->ReadSegments(StreamReaderCallback, (void *)this, aCount, &totalRead);
}

NS_IMETHODIMP
nsFeedLoadListener::OnStopRequest(nsIRequest *aRequest,
                                  nsISupports *aContext,
                                  nsresult aStatus)
{
    nsresult rv;

    if (mAborted) {
        mBMSVC->Unassert (mResource, kNC_LivemarkLock, kTrueLiteral);
        return NS_OK;
    }

    if (NS_FAILED(aStatus)) {
        // Something went wrong; try to load again in 5 minutes.
        SetResourceTTL (300);
        mBMSVC->Unassert (mResource, kNC_LivemarkLock, kTrueLiteral);
        return NS_OK;
    }

    /* Either turn the livemark into a Seq if it isn't one already, or clear out the
     * old data.
     */
    do {
        PRBool isContainer = PR_FALSE;
        rv = gRDFC->IsContainer(mInnerBMDataSource, mResource, &isContainer);
        if (NS_FAILED(rv)) break;

        if (!isContainer) {
            rv = gRDFC->MakeSeq(mInnerBMDataSource, mResource, getter_AddRefs(mLivemarkContainer));
            if (NS_FAILED(rv)) break;
        } else {
            rv = nsBMSVCClearSeqContainer(mInnerBMDataSource, mResource);
            if (NS_FAILED(rv)) break;

            mLivemarkContainer = do_CreateInstance (kRDFContainerCID, &rv);
            if (NS_FAILED(rv)) break;

            rv = mLivemarkContainer->Init (mInnerBMDataSource, mResource);
            if (NS_FAILED(rv)) break;
        }
    } while (0);

    if (NS_FAILED(rv)) {
        mBMSVC->Unassert (mResource, kNC_LivemarkLock, kTrueLiteral);
        return rv;
    }

    /* We need to parse the returned data here, stored in mBody.  We
     * try parsing as RDF first, then as Atom and the "simple" RSS
     * (the userland 0.91/0.92/2.0 formats)
     */

    /* Try parsing as RDF */
    rv = TryParseAsRDF ();

    /* Try parsing as Atom/Simple RSS */
    if (!NS_SUCCEEDED(rv)) {
        rv = TryParseAsSimpleRSS ();
    }

    /* If we weren't able to load with anything, attach a dummy bookmark */
    if (!NS_SUCCEEDED(rv)) {
        mLivemarkContainer->AppendElement(mBMSVC->mLivemarkLoadFailedBookmark);
    }

    /* Set an expiration on the livemark, for reloading the data */
    PRInt32 ttl = 3600;     // XXXvladimir FIXME: read ttl from rss
    if (NS_FAILED(rv)) {
        // if we failed, try again in 5 minutes, to avoid trying
        // to load the livemark over and over.
        ttl = 300;          
    }
    rv = SetResourceTTL(ttl);
    if (NS_FAILED(rv)) {
        NS_WARNING ("SetResourceTTL failed on Livemark");
    }

    rv = mBMSVC->Unassert (mResource, kNC_LivemarkLock, kTrueLiteral);
    if (NS_FAILED(rv)) {
        NS_WARNING ("LivemarkLock unassert failed!");
    }

    return NS_OK;
}

/**
 * SetResourceTTL: Set the next time we should attempt to reload this
 * resource's feed
 */

nsresult
nsFeedLoadListener::SetResourceTTL (PRInt32 aTTL)
{
    nsresult rv;

    PRTime million, temp64, exptime = PR_Now();
    LL_I2L (million, PR_USEC_PER_SEC);
    LL_I2L (temp64, aTTL);
    LL_MUL (temp64, temp64, million);
    LL_ADD (exptime, exptime, temp64);

    nsCOMPtr<nsIRDFDate> newNode;
    rv = gRDF->GetDateLiteral (exptime, getter_AddRefs(newNode));
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIRDFNode> oldNode;
    rv = mInnerBMDataSource->GetTarget (mResource, kNC_LivemarkExpiration, PR_TRUE, getter_AddRefs(oldNode));
    if (NS_FAILED(rv)) return rv;
    if (rv == NS_OK) {
        rv = mInnerBMDataSource->Change (mResource, kNC_LivemarkExpiration, oldNode, newNode);
    } else {
        rv = mInnerBMDataSource->Assert (mResource, kNC_LivemarkExpiration, newNode, PR_TRUE);
    }
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

/**
 * TryParseAsRDF: attempt to parse the read data in mBody as
 * RSS 0.90/1.0 data.  This is supposed to be RDF, so we use
 * the RDF parser to do our work for us.
 */

nsresult
nsFeedLoadListener::TryParseAsRDF ()
{
    nsresult rv;

    nsCOMPtr<nsIRDFXMLParser> rdfparser(do_CreateInstance("@mozilla.org/rdf/xml-parser;1", &rv));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRDFDataSource> ds(do_CreateInstance(kRDFInMemoryDataSourceCID, &rv));
    if (NS_FAILED(rv)) return rv;

    rv = rdfparser->ParseString (ds, mURI, mBody);
    if (NS_FAILED(rv)) return rv;

    // Grab the (only) thing that's a channel
    // We try RSS 1.0 first, then RSS 0.9, and set up the remaining
    // resources accordingly

    nsIRDFResource *RSS_items = nsnull;
    nsIRDFResource *RSS_title = nsnull;
    nsIRDFResource *RSS_link = nsnull;

    nsCOMPtr<nsIRDFResource> channelResource = nsnull;

    rv = ds->GetSource(kRDF_type, kRSS10_channel, PR_TRUE, getter_AddRefs(channelResource));
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    if (rv == NS_OK) {
        RSS_items = kRSS10_items;
        RSS_title = kRSS10_title;
        RSS_link = kRSS10_link;
    } else {
        // try RSS 0.9
        rv = ds->GetSource(kRDF_type, kRSS09_channel, PR_TRUE, getter_AddRefs(channelResource));
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
        if (rv == NS_OK) {
            RSS_items = nsnull;
            RSS_title = kRSS09_title;
            RSS_link = kRSS09_link;
        }
    }

    if (!channelResource) {
        // no channel, either 1.0 or 0.9
        return NS_ERROR_FAILURE;
    }

    // this will get filled in differently.
    nsCOMPtr<nsISimpleEnumerator> itemsEnumerator;

    if (RSS_items) {
        // if there is something that should be rss:items, then it's RSS 1.0
        nsCOMPtr<nsIRDFNode> itemsNode;
        rv = ds->GetTarget(channelResource, RSS_items, PR_TRUE, getter_AddRefs(itemsNode));
        if (NS_FAILED(rv) || rv == NS_RDF_NO_VALUE) return NS_ERROR_FAILURE;

        /* items is a seq */
        nsCOMPtr<nsIRDFContainer> itemsContainer = do_CreateInstance (kRDFContainerCID, &rv);
        if (NS_FAILED(rv)) return rv;
        rv = itemsContainer->Init(ds, (nsIRDFResource *) itemsNode.get());
        if (NS_FAILED(rv)) return rv;

        rv = itemsContainer->GetElements (getter_AddRefs(itemsEnumerator));
        if (NS_FAILED(rv) || rv == NS_RDF_NO_VALUE) return NS_ERROR_FAILURE;
    } else {
        //
        // if there is no rss:items, but we still were able to parse it as RDF
        // and found a channel, then it's possibly RSS 0.9.  For RSS 0.9,
        // we know that each item will be an <item ...>, so we get everything
        // that has a type of item.
        rv = ds->GetSources(kRDF_type, kRSS09_item, PR_TRUE, getter_AddRefs(itemsEnumerator));
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    }

    /* Go through each resource and pull out its link/title, if present. */
    PRBool more;
    while (NS_SUCCEEDED(rv = itemsEnumerator->HasMoreElements(&more)) && more) {
        nsCOMPtr<nsISupports> iSupports;
        rv = itemsEnumerator->GetNext(getter_AddRefs(iSupports));
        if (NS_FAILED(rv)) break;

        nsCOMPtr<nsIRDFResource> item(do_QueryInterface(iSupports));
        if (!item) {
            rv = NS_ERROR_UNEXPECTED;
            break;
        }

        rv = HandleRDFItem (ds, item, RSS_link, RSS_title);
        // ignore rv
    }
    if (NS_FAILED(rv)) return rv;

#ifdef DEBUG_vladimir
    NS_WARNING (">>>> Success from TryParseAsRDF!\n");
#endif

    return NS_OK;
}

//
// find the first node below aParentNode that is a TEXT_NODE,
// and return it
//
nsresult
nsFeedLoadListener::FindTextNode (nsIDOMNode *aParentNode, nsIDOMNode **aTextNode)
{
    NS_ENSURE_ARG(aParentNode);
    NS_ENSURE_ARG(aTextNode);

    nsresult rv;

    *aTextNode = nsnull;

    nsCOMPtr<nsIDOMNode> childNode;
    rv = aParentNode->GetFirstChild(getter_AddRefs(childNode));
    if (!childNode) return NS_OK;
    if (NS_FAILED(rv)) return rv;

    PRUint16 nodeType = 0;
    do {
        rv = childNode->GetNodeType(&nodeType);
        if (nodeType == nsIDOMNode::TEXT_NODE)
            break;

        nsCOMPtr<nsIDOMNode> temp;
        rv = childNode->GetNextSibling(getter_AddRefs(temp));
        if (NS_FAILED(rv)) return rv;
        childNode = temp;
    } while (childNode);

    if (nodeType == nsIDOMNode::TEXT_NODE) {
        *aTextNode = childNode.get();
        NS_ADDREF(*aTextNode);
    }

    return NS_OK;
}

nsresult
nsFeedLoadListener::HandleRDFItem (nsIRDFDataSource *aDS, nsIRDFResource *aItem,
                                   nsIRDFResource *aLinkResource,
                                   nsIRDFResource *aTitleResource)
{
    nsresult rv;

    /* We care about this item's link and title */
    nsCOMPtr<nsIRDFNode> linkNode;
    rv = aDS->GetTarget (aItem, aLinkResource, PR_TRUE, getter_AddRefs(linkNode));
    if (NS_FAILED(rv) || rv == NS_RDF_NO_VALUE) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIRDFNode> titleNode;
    rv = aDS->GetTarget (aItem, aTitleResource, PR_TRUE, getter_AddRefs(titleNode));
    if (NS_FAILED(rv) || rv == NS_RDF_NO_VALUE) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIRDFLiteral> linkLiteral(do_QueryInterface(linkNode));
    nsCOMPtr<nsIRDFLiteral> titleLiteral(do_QueryInterface(titleNode));

    const PRUnichar *linkStr, *titleStr;
    rv = linkLiteral->GetValueConst(&linkStr);
    rv |= titleLiteral->GetValueConst(&titleStr);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRDFResource> newBM;
    rv = mBMSVC->CreateBookmark (titleStr, linkStr, nsnull, nsnull, nsnull, nsnull,
                                 getter_AddRefs(newBM));
    if (NS_FAILED(rv)) return rv;

    rv = mLivemarkContainer->AppendElement(newBM);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

/**
 * TryParseAsSimpleRSS
 *
 * Tries to parse the content as RSS (Userland) 0.91/0.92/2.0, or Atom
 * These are not RDF formats.
 */

nsresult
nsFeedLoadListener::TryParseAsSimpleRSS ()
{
    nsresult rv;

    nsCOMPtr<nsIDOMParser> parser(do_CreateInstance("@mozilla.org/xmlextras/domparser;1", &rv));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIDOMDocument> xmldoc;
    rv = parser->ParseFromUTF8String (mBody, NS_LITERAL_CSTRING("text/xml"), getter_AddRefs(xmldoc));
    if (NS_FAILED(rv)) return rv;

    // becomes true if we figure out that this is an atom stream.
    PRBool isAtom = PR_FALSE;

    nsCOMPtr<nsIDOMElement> docElement;
    rv = xmldoc->GetDocumentElement(getter_AddRefs(docElement));
    if (!docElement) return NS_ERROR_UNEXPECTED;
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIDOMNode> node;
    rv = xmldoc->GetFirstChild(getter_AddRefs(node));
    if (!node) return NS_ERROR_UNEXPECTED;
    if (NS_FAILED(rv)) return rv;

    PRBool lookingForChannel = PR_FALSE;

    while (node) {
        PRUint16 ntype;
        rv = node->GetNodeType(&ntype);
        if (NS_FAILED(rv)) return rv;

        if (ntype == nsIDOMNode::ELEMENT_NODE) {
            nsAutoString nname;
            rv = node->GetNodeName (nname);

            /* slight hack to get node pointing to the thing that
             * has items/entries as its children.  We need to descend
             * into <channel> for RSS, but not for Atom.
             */
            if (!lookingForChannel) {
                if (nname.Equals(NS_LITERAL_STRING("rss"))) {
                    lookingForChannel = PR_TRUE;
                    nsCOMPtr<nsIDOMNode> temp;
                    rv = node->GetFirstChild(getter_AddRefs(temp));
                    if (!temp) return NS_ERROR_UNEXPECTED;
                    if (NS_FAILED(rv)) return rv;
                    node = temp;
                    continue;
                }
                if (nname.Equals(NS_LITERAL_STRING("feed"))) {
                    /* Atom has no <channel>; instead, <item>s are
                     * children of <feed> */
                    isAtom = PR_TRUE;
                    break;
                }
            } else {
                if (nname.Equals(NS_LITERAL_STRING("channel"))) {
                    break;
                }
            }
        }

        nsCOMPtr<nsIDOMNode> temp;
        rv = node->GetNextSibling(getter_AddRefs(temp));
        if (!temp) return NS_ERROR_UNEXPECTED;
        if (NS_FAILED(rv)) return rv;
        node = temp;
    }

    // we didn't find a rss/feed/channel or whatever
    if (!node)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDOMElement> chElement = do_QueryInterface(node);
    if (!chElement) return NS_ERROR_UNEXPECTED;

    /* Go through the <channel>/<feed> and do what we need
     * with <item> or <entry> nodes
     */
    int numMarksAdded = 0;

    rv = chElement->GetFirstChild(getter_AddRefs(node));
    if (!node) return NS_ERROR_UNEXPECTED;
    if (NS_FAILED(rv)) return rv;

    while (node) {
        PRUint16 ntype;
        rv = node->GetNodeType(&ntype);
        if (NS_FAILED(rv)) return rv;

        if (ntype == nsIDOMNode::ELEMENT_NODE) {
            nsAutoString nname;
            rv = node->GetNodeName (nname);

            if ((!isAtom && nname.Equals(NS_LITERAL_STRING("item"))) ||
                ( isAtom && nname.Equals(NS_LITERAL_STRING("entry"))))
            {
                /* We need to pull out the <title> and <link> children */
                nsAutoString titleStr;
                nsAutoString linkStr;

                nsCOMPtr<nsIDOMNode> childNode;
                rv = node->GetFirstChild(getter_AddRefs(childNode));
                if (NS_FAILED(rv)) return rv;

                while (childNode) {
                    PRUint16 childNtype;
                    rv = childNode->GetNodeType(&childNtype);
                    if (NS_FAILED(rv)) return rv;

                    if (childNtype == nsIDOMNode::ELEMENT_NODE) {
                        nsAutoString childNname;
                        rv = childNode->GetNodeName (childNname);

                        if (childNname.Equals(NS_LITERAL_STRING("title"))) {
                            nsCOMPtr<nsIDOMNode> textNode;

                            rv = FindTextNode (childNode, getter_AddRefs(textNode));
                            if (!textNode || NS_FAILED(rv)) break;

                            nsCOMPtr<nsIDOMCharacterData> charTextNode = do_QueryInterface(textNode);
                            charTextNode->GetData(titleStr);
                        } else if (childNname.Equals(NS_LITERAL_STRING("link"))) {
                            if (!isAtom) {
                                // in node's TEXT
                                nsCOMPtr<nsIDOMNode> textNode;

                                rv = FindTextNode (childNode, getter_AddRefs(textNode));
                                if (!textNode || NS_FAILED(rv)) break;

                                nsCOMPtr<nsIDOMCharacterData> charTextNode = do_QueryInterface(textNode);
                                charTextNode->GetData(linkStr);
                            } else {
                                // in HREF attribute
                                nsCOMPtr<nsIDOMElement> linkElem = do_QueryInterface(childNode);
                                if (!linkElem) break; // out of while(childNode) loop

                                rv = linkElem->GetAttribute(NS_LITERAL_STRING("href"), linkStr);
                                if (NS_FAILED(rv)) break; // out of while(childNode) loop
                            }
                        }
                    }

                    if (!titleStr.IsEmpty() && !linkStr.IsEmpty())
                        break;

                    nsCOMPtr<nsIDOMNode> temp;
                    rv = childNode->GetNextSibling(getter_AddRefs(temp));
                    childNode = temp;
                    if (!childNode || NS_FAILED(rv)) break;
                }

                if (!titleStr.IsEmpty() && !linkStr.IsEmpty()) {
                    nsCOMPtr<nsIRDFResource> newBM;
                    rv = mBMSVC->CreateBookmark (titleStr.get(), linkStr.get(),
                                                 nsnull, nsnull, nsnull, nsnull,
                                                 getter_AddRefs(newBM));
                    if (NS_FAILED(rv)) return rv;

                    rv = mLivemarkContainer->AppendElement(newBM);
                    if (NS_FAILED(rv)) return rv;

                    numMarksAdded++;
                }
            }
        }

        nsCOMPtr<nsIDOMNode> temp;
        rv = node->GetNextSibling(getter_AddRefs(temp));
        if (NS_FAILED(rv)) return rv;
        node = temp;
    }


    if (numMarksAdded > 0) {
#ifdef DEBUG_vladimir
        NS_WARNING (">>>> Success from TryParseAsSimpleRSS!\n");
#endif
        return NS_OK;
    }

    return NS_ERROR_FAILURE;
}


///////////////////////////////////////////////////////////////////////////
////  Main entry point for nsBookmarksService to deal with Livemarks
////

/*
 * Update the child elements of a livemark; take care of cache checking,
 * channel setup and firing off the async load and parse.
 */
nsresult
nsBookmarksService::UpdateLivemarkChildren(nsIRDFResource* aSource)
{
    nsresult rv;

    // Check whether we have a Feed URL first, before locking;
    // we'll hit this often while we're making a new livemark,
    // and no sense in going through the lock/unlock cycle
    // umpteen times.
    nsCOMPtr<nsIRDFNode> feedUrlNode;
    rv = mInner->GetTarget(aSource, kNC_FeedURL, PR_TRUE, getter_AddRefs(feedUrlNode));
    if (NS_FAILED(rv)) return rv;

    // if there's no feed attached, ignore this for now; hopefully
    // one will get attached soon.
    if (rv == NS_RDF_NO_VALUE)
        return NS_OK;

    nsCOMPtr<nsIRDFLiteral> feedUrlLiteral = do_QueryInterface(feedUrlNode);
    const PRUnichar *feedUrl = nsnull;
    rv = feedUrlLiteral->GetValueConst(&feedUrl);
    if (NS_FAILED(rv)) return rv;

    nsCString feedUrlString = NS_ConvertUTF16toUTF8(feedUrl);

    if (feedUrlString.IsEmpty())
        return rv;

#define UNLOCK_AND_RETURN_RV do { Unassert (aSource, kNC_LivemarkLock, kTrueLiteral); return rv; } while (0)

    PRBool locked = PR_FALSE;
    if (NS_SUCCEEDED(mInner->HasAssertion (aSource, kNC_LivemarkLock, kTrueLiteral, PR_TRUE, &locked)) &&
        locked)
    {
        /* We're already loading the livemark */
        return NS_OK;
    }

    rv = mInner->Assert (aSource, kNC_LivemarkLock, kTrueLiteral, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    // Check the TTL/expiration on this.  If there isn't one,
    // then we assume it's never been loaded.
    nsCOMPtr<nsIRDFNode> expirationNode;
    rv = mInner->GetTarget(aSource, kNC_LivemarkExpiration, PR_TRUE, getter_AddRefs(expirationNode));
    if (rv == NS_OK) {
        nsCOMPtr<nsIRDFDate> expirationTime = do_QueryInterface (expirationNode);
        PRTime exprTime, nowTime = PR_Now();
        expirationTime->GetValue(&exprTime);

        if (exprTime > nowTime) {
            // no need to refresh yet
            rv = Unassert (aSource, kNC_LivemarkLock, kTrueLiteral);
            if (NS_FAILED(rv)) return rv;

            return NS_OK;
        }
    } else {
        do {
            // it's never been loaded.  add a dummy "Livemark Loading..." entry
            nsCOMPtr<nsIRDFContainer> container(do_CreateInstance(kRDFContainerCID, &rv));
            if (NS_FAILED(rv)) break;
            rv = container->Init(mInner, aSource);
            if (NS_FAILED(rv)) break;
            rv = container->AppendElement(mLivemarkLoadingBookmark);
        } while (0);
    }

    nsCOMPtr<nsIURI> uri;
    nsCOMPtr<nsIChannel> uriChannel;
    rv = NS_NewURI(getter_AddRefs(uri), feedUrlString, nsnull, nsnull);
    if (NS_FAILED(rv)) UNLOCK_AND_RETURN_RV;

    rv = NS_NewChannel(getter_AddRefs(uriChannel), uri, nsnull, nsnull, nsnull,
                       nsIRequest::LOAD_BACKGROUND);
    if (NS_FAILED(rv)) UNLOCK_AND_RETURN_RV;

    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(uriChannel);
    if (httpChannel) {
        // XXXvladimir - handle POST livemarks
        rv = httpChannel->SetRequestMethod(NS_LITERAL_CSTRING("GET"));
    }
    
    nsCOMPtr<nsFeedLoadListener> listener = new nsFeedLoadListener(this, mInner, uri, aSource);

    nsCOMPtr<nsIChannel> channel;
    rv = NS_NewChannel(getter_AddRefs(channel), uri, nsnull, nsnull, nsnull,
                       nsIRequest::LOAD_BACKGROUND);
    if (NS_FAILED(rv)) UNLOCK_AND_RETURN_RV;

    rv = channel->AsyncOpen(listener, nsnull);
    if (NS_FAILED(rv)) UNLOCK_AND_RETURN_RV;

    return NS_OK;

#undef UNLOCK_AND_RETURN_RV
}


///////////////////////////////////////////////////////////////////////////
////  Utility methods

/* Clear out all elements from the container */
/* FIXME - why is there no RDFC method to clear a container? */
nsresult
nsBMSVCClearSeqContainer (nsIRDFDataSource* aDataSource, nsIRDFResource* aResource)
{
    nsresult rv;

    nsCOMPtr<nsIRDFContainer> itemsContainer = do_CreateInstance (kRDFContainerCID, &rv);
    rv = itemsContainer->Init (aDataSource, aResource);
    if (NS_FAILED(rv)) return rv;

    PRInt32 itemsCount = 0;
    rv = itemsContainer->GetCount(&itemsCount);
    if (NS_FAILED(rv)) return rv;
    if (itemsCount) {
        do {
            nsIRDFNode *removed;
            rv = itemsContainer->RemoveElementAt(itemsCount, PR_TRUE, &removed);
            // er, ignore the error, I think
        } while (--itemsCount > 0);
    }
    return NS_OK;
}

nsresult
nsBMSVCUnmakeSeq (nsIRDFDataSource* aDataSource, nsIRDFResource* aResource)
{
    nsresult rv;

    rv = nsBMSVCClearSeqContainer(aDataSource, aResource);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIRDFNode> old;

    rv = aDataSource->GetTarget(aResource, kRDF_nextVal, PR_TRUE, getter_AddRefs(old));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aDataSource->Unassert(aResource, kRDF_nextVal, (nsIRDFResource*) old.get());
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIRDFResource> RDF_instanceOf;
    nsCOMPtr<nsIRDFResource> RDF_Seq;
    gRDF->GetResource(NS_LITERAL_CSTRING(RDF_NAMESPACE_URI "instanceOf"), getter_AddRefs(RDF_instanceOf));
    gRDF->GetResource(NS_LITERAL_CSTRING(RDF_NAMESPACE_URI "Seq"), getter_AddRefs(RDF_Seq));

    rv = aDataSource->Unassert(aResource, RDF_instanceOf, RDF_Seq.get());
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

