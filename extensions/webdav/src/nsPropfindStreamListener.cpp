/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:expandtab:ts=4 sw=4:
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is Oracle Corporation.
 * Portions created by Oracle Corporation are  Copyright (C) 2004
 * by Oracle Corporation.  All Rights Reserved.
 *
 * Contributor(s):
 *   Mike Shaver <shaver@off.net> (original author)
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIHttpChannel.h"
#include "nsIIOService.h"
#include "nsNetUtil.h"

#include "nsIDOM3Node.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMParser.h"
#include "nsIDOMRange.h"

#include "nsIDocument.h"
#include "nsIDocumentEncoder.h"

#include "nsIWebDAVResource.h"
#include "nsIWebDAVListener.h"

#include "nsWebDAVInternal.h"

#include "nsISupportsPrimitives.h"

class PropfindStreamListener : public nsIStreamListener
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER

    PropfindStreamListener(nsIWebDAVResource *resource,
                           nsIWebDAVMetadataListener *listener) :
        mResource(resource), mListener(listener) { }
    virtual ~PropfindStreamListener() { }
protected:

    static NS_METHOD StreamReaderCallback(nsIInputStream *in, void *closure,
                                          const char *fromRawSegment,
                                          PRUint32 toOffset, PRUint32 count,
                                          PRUint32 *writeCount);

    NS_METHOD SignalCompletion(PRUint32 aStatusCode)
    {
        if (LOG_ENABLED()) {
            nsCAutoString spec("");
            mResource->GetUrlSpec(spec);
            LOG(("PROPFIND completed for %s: %d", spec.get(), aStatusCode));
        }
        mListener->OnMetadataComplete(aStatusCode, mResource, "PROPFIND");
        return NS_OK;
    }

    NS_METHOD ProcessResponse(nsIDOMElement *responseElt);
    NS_METHOD PropertiesFromPropElt(nsIDOMElement *elt,
                                    nsIProperties **retProps);
    
    nsCOMPtr<nsIWebDAVResource>         mResource;
    nsCOMPtr<nsIWebDAVMetadataListener> mListener;
    nsCOMPtr<nsIDOMDocument>            mXMLDoc;
    nsCString                           mBody;
};

NS_IMPL_ISUPPORTS1(PropfindStreamListener, nsIStreamListener)

NS_IMETHODIMP
PropfindStreamListener::OnStartRequest(nsIRequest *aRequest,
                                       nsISupports *aContext)
{
    mBody.Truncate();
    return NS_OK;
}

nsresult
PropfindStreamListener::PropertiesFromPropElt(nsIDOMElement *propElt,
                                              nsIProperties **retProps)
{
    nsresult rv = CallCreateInstance(NS_PROPERTIES_CONTRACTID, retProps);
    NS_ENSURE_SUCCESS(rv, rv);

    nsIProperties *props = *retProps;

    nsCOMPtr<nsIDOMNodeList> list;
    rv = propElt->GetChildNodes(getter_AddRefs(list));
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint32 length;
    rv = list->GetLength(&length);
    NS_ENSURE_SUCCESS(rv, rv);

    LOG(("%d properties found", length));

    PRUint32 realProps = 0;

    for (PRUint32 i = 0; i < length; i++) {
        nsCOMPtr<nsIDOMNode> node;
        nsCOMPtr<nsIDOM3Node> node3;
        rv = list->Item(i, getter_AddRefs(node));
        NS_ENSURE_SUCCESS(rv, rv);

        PRUint16 type;
        node->GetNodeType(&type);
        if (type != nsIDOMNode::ELEMENT_NODE)
            continue;

        realProps++;
        
        nsCOMPtr<nsIDOMRange> range = 
          do_CreateInstance("@mozilla.org/content/range;1", &rv);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = range->SelectNodeContents(node);
        NS_ENSURE_SUCCESS(rv, rv);
        
        nsString nsStr;
        rv = node->GetNamespaceURI(nsStr);
        NS_ENSURE_SUCCESS(rv, rv);

        nsString propName;
        rv = node->GetLocalName(propName);
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<nsIDocumentEncoder> encoder =
          do_CreateInstance(NS_DOC_ENCODER_CONTRACTID_BASE "text/xml", &rv);

        nsCOMPtr<nsIDocument> baseDoc = do_QueryInterface(mXMLDoc, &rv);
        NS_ENSURE_SUCCESS(rv, rv);

        // This method will fail if no document
        rv = encoder->Init(baseDoc, NS_LITERAL_STRING("text/xml"),
                           nsIDocumentEncoder::OutputEncodeBasicEntities);
        NS_ENSURE_SUCCESS(rv, rv);
        
        rv = encoder->SetRange(range);
        NS_ENSURE_SUCCESS(rv, rv);

        nsString valueStr;
        encoder->EncodeToString(valueStr);

        nsCOMPtr<nsISupportsString>
            suppString(do_CreateInstance("@mozilla.org/supports-string;1",
                                         &rv));
        NS_ENSURE_SUCCESS(rv, rv);
        suppString->SetData(valueStr);

        NS_ConvertUTF16toUTF8 propkey(nsStr + NS_LITERAL_STRING(" ") +
                                      propName);
        LOG(("  %s = %s", propkey.get(),
             NS_ConvertUTF16toUTF8(valueStr).get()));
        rv = props->Set(propkey.get(), suppString);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    LOG(("%d real properties added", realProps));
    
    return NS_OK;
}

nsresult
PropfindStreamListener::ProcessResponse(nsIDOMElement *responseElt)
{
    nsCOMPtr<nsIDOMElement> hrefElt;

    nsString hrefString;
    nsresult rv = NS_WD_ElementTextChildValue(responseElt,
                                              NS_LITERAL_STRING("href"),
                                              hrefString);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ConvertUTF16toUTF8 hrefUTF8(hrefString);

    LOG(("response for %s", hrefUTF8.get()));

    nsCOMPtr<nsIDOMNodeList> proplist;
    rv = responseElt->GetElementsByTagName(NS_LITERAL_STRING("propstat"),
                                           getter_AddRefs(proplist));
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint32 length;
    rv = proplist->GetLength(&length);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMNode> node;
    for (PRUint32 i = 0; i < length; i++) {
        rv = proplist->Item(i, getter_AddRefs(node));
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<nsIDOMElement> propstatElt = do_QueryInterface(node, &rv);
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<nsIDOMElement> elt;
        rv = NS_WD_GetElementByTagName(propstatElt, NS_LITERAL_STRING("prop"),
                                       getter_AddRefs(elt));
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<nsIProperties> props;
        rv = PropertiesFromPropElt(elt, getter_AddRefs(props));
        NS_ENSURE_SUCCESS(rv, rv);
        
        nsString statusString;
        rv = NS_WD_ElementTextChildValue(propstatElt,
                                         NS_LITERAL_STRING("status"),
                                         statusString);
        NS_ENSURE_SUCCESS(rv, rv);

        PRInt32 res = 0;
        NS_ConvertUTF16toUTF8 statusUTF8(statusString);
        LOG(("status: %s", statusUTF8.get()));
        PRInt32 statusVal = nsCAutoString(Substring(statusUTF8, 8)).ToInteger(&res, 10);
        NS_ENSURE_SUCCESS(res, (nsresult)res);
        
        mListener->OnGetPropertiesResult((PRUint32)statusVal, hrefUTF8, props);
    }
    return NS_OK;
}

NS_IMETHODIMP
PropfindStreamListener::OnStopRequest(nsIRequest *aRequest,
                                      nsISupports *aContext,
                                      nsresult aStatusCode)
{
    PRUint32 status, rv;
    nsCOMPtr<nsIHttpChannel> channel = do_QueryInterface(aContext);

    rv = channel ? channel->GetResponseStatus(&status) : NS_ERROR_UNEXPECTED;

    if (NS_FAILED(rv))
        return SignalCompletion(rv);

    if (status != 207)
        return SignalCompletion(status);

    // ZZZ Check content-length against mBody.Length()

    // Now we parse!

    nsCOMPtr<nsIDOMParser> 
        parser(do_CreateInstance("@mozilla.org/xmlextras/domparser;1", &rv));
    NS_ENSURE_SUCCESS(rv, SignalCompletion(rv));

    nsCOMPtr<nsIDOMDocument> xmldoc;
    rv = parser->ParseFromBuffer(NS_REINTERPRET_CAST(const PRUint8 *,
                                                     mBody.get()),
                                 mBody.Length(), "text/xml",
                                 getter_AddRefs(mXMLDoc));
    NS_ENSURE_SUCCESS(rv, SignalCompletion(rv));

    nsCOMPtr<nsIDOMNodeList> responseList;
    rv = mXMLDoc->GetElementsByTagNameNS(NS_LITERAL_STRING("DAV:"),
                                         NS_LITERAL_STRING("response"),
                                         getter_AddRefs(responseList));
    NS_ENSURE_SUCCESS(rv, SignalCompletion(rv));

    PRUint32 length;
    rv = responseList->GetLength(&length);
    NS_ENSURE_SUCCESS(rv, SignalCompletion(rv));

    LOG(("found %d responses", length));

    NS_ENSURE_TRUE(length, SignalCompletion(NS_ERROR_UNEXPECTED));
    
    for (PRUint32 i = 0; i < length; i++) {
        nsCOMPtr<nsIDOMNode> responseNode;
        rv = responseList->Item(i, getter_AddRefs(responseNode));
        NS_ENSURE_SUCCESS(rv, SignalCompletion(rv));

        nsCOMPtr<nsIDOMElement> responseElt =
            do_QueryInterface(responseNode, &rv);
        NS_ENSURE_SUCCESS(rv, SignalCompletion(rv));
    
        rv = ProcessResponse(responseElt);
        NS_ENSURE_SUCCESS(rv, SignalCompletion(rv));
    }

    SignalCompletion(status);
    return NS_OK;
}

NS_METHOD
PropfindStreamListener::StreamReaderCallback(nsIInputStream *aInputStream,
                                             void *aClosure,
                                             const char *aRawSegment,
                                             PRUint32 aToOffset,
                                             PRUint32 aCount,
                                             PRUint32 *aWriteCount)
{
    PropfindStreamListener *psl = NS_STATIC_CAST(PropfindStreamListener *,
                                                 aClosure);
    psl->mBody.Append(aRawSegment, aCount);
    *aWriteCount = aCount;
    return NS_OK;
}

NS_IMETHODIMP
PropfindStreamListener::OnDataAvailable(nsIRequest *aRequest,
                                        nsISupports *aContext,
                                        nsIInputStream *aInputStream,
                                        PRUint32 aOffset, PRUint32 aCount)
{
    PRUint32 result;
    nsCOMPtr<nsIHttpChannel> channel = do_QueryInterface(aContext);
    if (!channel)
        result = NS_ERROR_UNEXPECTED;

    PRBool succeeded = PR_FALSE;
    channel->GetRequestSucceeded(&succeeded);
    if (!succeeded) {
        aRequest->Cancel(NS_BINDING_ABORTED);
        return NS_BINDING_ABORTED;
    }

    PRUint32 totalRead;
    return aInputStream->ReadSegments(StreamReaderCallback, (void *)this,
                                      aCount, &totalRead);
}

nsIStreamListener *
NS_NewPropfindStreamListener(nsIWebDAVResource *resource,
                             nsIWebDAVMetadataListener *listener)
{
  return new PropfindStreamListener(resource, listener);
}
