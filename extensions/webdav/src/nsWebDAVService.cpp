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
 *
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

#if defined(MOZ_LOGGING)
#define FORCE_PR_LOG
#endif

#include "nsWebDAVInternal.h"

#include "nsIWebDAVService.h"
#include "nsWebDAVServiceCID.h"

#include "nsIServiceManagerUtils.h"

#include "nsIHttpChannel.h"
#include "nsIIOService.h"
#include "nsNetUtil.h"
#include "nsIStorageStream.h"
#include "nsIUploadChannel.h"

#include "nsContentCID.h"

#include "nsIDOMXMLDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsIDOM3Node.h"
#include "nsIPrivateDOMImplementation.h" // I don't even pretend any more
#include "nsIDOMDOMImplementation.h"

#include "nsIDocument.h"
#include "nsIDocumentEncoder.h"

#include "nsIDOMParser.h"

#include "nsIGenericFactory.h"

class nsWebDAVService : public nsIWebDAVService
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBDAVSERVICE
    
    nsWebDAVService();
    virtual ~nsWebDAVService();
protected:
    nsresult EnsureIOService();
    nsresult ChannelFromResource(nsIWebDAVResource *resource,
                                 nsIHttpChannel** channel,
                                 nsIURI ** resourceURI = 0);

    nsresult CreatePropfindDocument(nsIURI *resourceURI,
                                    nsIDOMDocument **requestDoc,
                                    nsIDOMElement **propfindElt);

    nsresult PropfindInternal(nsIWebDAVResource *resource, PRUint32 propCount,
                              const char **properties, PRBool withDepth,
                              nsIWebDAVOperationListener *listener,
                              PRBool namesOnly);

    nsresult SendPropfindDocumentToChannel(nsIDocument *doc,
                                           nsIHttpChannel *channel,
                                           nsIStreamListener *listener,
                                           PRBool withDepth);
    nsCOMPtr<nsIIOService> mIOService; // XXX weak?
    nsAutoString mDAVNSString; // "DAV:"
};

NS_IMPL_ISUPPORTS1_CI(nsWebDAVService, nsIWebDAVService)

#define ENSURE_IO_SERVICE()                     \
{                                               \
    nsresult rv_ = EnsureIOService();           \
    NS_ENSURE_SUCCESS(rv_, rv_);                \
}

nsresult
nsWebDAVService::EnsureIOService()
{
    if (!mIOService) {
        nsresult rv;
        mIOService = do_GetIOService(&rv);
        if (!mIOService)
            return rv;
    }

    return NS_OK;
}

nsresult
nsWebDAVService::SendPropfindDocumentToChannel(nsIDocument *doc,
                                               nsIHttpChannel *channel,
                                               nsIStreamListener *listener,
                                               PRBool withDepth)
{
    nsCOMPtr<nsIStorageStream> storageStream;
    // Why do I have to pick values for these?  I just want to store some data
    // for stream access!  (And how would script set these?)
    nsresult rv = NS_NewStorageStream(4 * 1024, 256 * 1024,
                             getter_AddRefs(storageStream));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIOutputStream> storageOutputStream;
    rv = storageStream->GetOutputStream(0,
                                        getter_AddRefs(storageOutputStream));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDocumentEncoder> encoder =
        do_CreateInstance(NS_DOC_ENCODER_CONTRACTID_BASE "text/xml", &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    
    rv = encoder->Init(doc, NS_LITERAL_STRING("text/xml"),
                       nsIDocumentEncoder::OutputEncodeBasicEntities);
    NS_ENSURE_SUCCESS(rv, rv);

    encoder->SetCharset(NS_LITERAL_CSTRING("UTF-8"));
    rv =  encoder->EncodeToStream(storageOutputStream);
    NS_ENSURE_SUCCESS(rv, rv);

    storageOutputStream->Close();

    // You gotta really want it.
    if (PR_LOG_TEST(gDAVLog, 5)) {
        nsCOMPtr<nsIInputStream> logInputStream;
        rv = storageStream->NewInputStream(0, getter_AddRefs(logInputStream));
        NS_ENSURE_SUCCESS(rv, rv);

        PRUint32 len, read;
        logInputStream->Available(&len);

        char *buf = new char[len+1];
        memset(buf, 0, len+1);
        logInputStream->Read(buf, len, &read);
        NS_ASSERTION(len == read, "short read on closed storage stream?");
        LOG(("XML:\n\n%*s\n\n", len, buf));
        
        delete [] buf;
    }

    nsCOMPtr<nsIInputStream> inputStream;
    rv = storageStream->NewInputStream(0, getter_AddRefs(inputStream));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIUploadChannel> uploadChannel = do_QueryInterface(channel, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = uploadChannel->SetUploadStream(inputStream,
                                        NS_LITERAL_CSTRING("text/xml"), -1);
    NS_ENSURE_SUCCESS(rv, rv);
    
    channel->SetRequestMethod(NS_LITERAL_CSTRING("PROPFIND"));

    // XXX I wonder how many compilers this will break...
    if (withDepth) {
        channel->SetRequestHeader(NS_LITERAL_CSTRING("Depth"),
                                  NS_LITERAL_CSTRING("1"), false);
    } else {
        channel->SetRequestHeader(NS_LITERAL_CSTRING("Depth"),
                                  NS_LITERAL_CSTRING("0"), false);
    }

    if (LOG_ENABLED()) {
        nsCOMPtr<nsIURI> uri;
        channel->GetURI(getter_AddRefs(uri));
        nsCAutoString spec;
        uri->GetSpec(spec);
        LOG(("PROPFIND starting for %s", spec.get()));
    }

    return channel->AsyncOpen(listener, channel);
}

nsresult
nsWebDAVService::CreatePropfindDocument(nsIURI *resourceURI,
                                        nsIDOMDocument **requestDoc,
                                        nsIDOMElement **propfindElt)
{
    nsresult rv;
    static NS_DEFINE_CID(kDOMDOMDOMDOMImplementationCID,
                         NS_DOM_IMPLEMENTATION_CID);
    nsCOMPtr<nsIDOMDOMImplementation>
        implementation(do_CreateInstance(kDOMDOMDOMDOMImplementationCID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIPrivateDOMImplementation> 
        privImpl(do_QueryInterface(implementation));
    privImpl->Init(resourceURI);

    nsCOMPtr<nsIDOMDocument> doc;
    nsAutoString emptyString;
    rv = implementation->CreateDocument(mDAVNSString, emptyString, nsnull,
                                        getter_AddRefs(doc));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDocument> baseDoc = do_QueryInterface(doc);
    baseDoc->SetXMLDeclaration(NS_LITERAL_STRING("1.0"), emptyString,
                               emptyString);
    baseDoc->SetDocumentURI(resourceURI);

    nsCOMPtr<nsIDOMElement> elt;
    rv = NS_WD_AppendElementWithNS(doc, doc, mDAVNSString,
                                   NS_LITERAL_STRING("propfind"),
                                   getter_AddRefs(elt));
    elt->SetPrefix(NS_LITERAL_STRING("D"));
    NS_ENSURE_SUCCESS(rv, rv);

    *requestDoc = doc.get();
    NS_ADDREF(*requestDoc);
    *propfindElt = elt.get();
    NS_ADDREF(*propfindElt);

    return NS_OK;
}

nsresult
nsWebDAVService::ChannelFromResource(nsIWebDAVResource *resource,
                                     nsIHttpChannel **channel,
                                     nsIURI **resourceURI)
{
    ENSURE_IO_SERVICE();

    nsCAutoString spec;

    nsresult rv = resource->GetUrlSpec(spec);
    NS_ENSURE_SUCCESS(rv, rv);
    if (spec.IsEmpty()) {
        NS_ASSERTION(0, "non-empty spec!");
        return NS_ERROR_INVALID_ARG;
    }

    nsCOMPtr<nsIURI> uri;
    rv = mIOService->NewURI(spec, nsnull, nsnull, getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIChannel> baseChannel;
    rv = mIOService->NewChannelFromURI(uri, getter_AddRefs(baseChannel));
    NS_ENSURE_SUCCESS(rv, rv);

    if (resourceURI) {
        *resourceURI = uri.get();
        NS_ADDREF(*resourceURI);
    }

    return CallQueryInterface(baseChannel, channel);
}

nsWebDAVService::nsWebDAVService() :
    mDAVNSString(NS_LITERAL_STRING("DAV:"))

{
    gDAVLog = PR_NewLogModule("webdav");
}

nsWebDAVService::~nsWebDAVService()
{
  /* destructor code */
}

NS_IMETHODIMP
nsWebDAVService::LockResources(PRUint32 count, nsIWebDAVResource **resources,
                               nsIWebDAVOperationListener *listener)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWebDAVService::UnlockResources(PRUint32 count, nsIWebDAVResource **resources,
                                 nsIWebDAVOperationListener *listener)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWebDAVService::GetResourcePropertyNames(nsIWebDAVResource *resource,
                                          PRBool withDepth,
                                          nsIWebDAVOperationListener *listener)
{
    return PropfindInternal(resource, 0, nsnull, withDepth,
                            listener, PR_TRUE);
}

NS_IMETHODIMP
nsWebDAVService::GetResourceProperties(nsIWebDAVResource *resource,
                                       PRUint32 propCount,
                                       const char **properties,
                                       PRBool withDepth,
                                       nsIWebDAVOperationListener *listener)
{
    return PropfindInternal(resource, propCount, properties, withDepth,
                            listener, PR_FALSE);
}

nsresult
nsWebDAVService::PropfindInternal(nsIWebDAVResource *resource,
                                  PRUint32 propCount,
                                  const char **properties,
                                  PRBool withDepth,
                                  nsIWebDAVOperationListener *listener,
                                  PRBool namesOnly)
{
    nsresult rv;

    NS_ENSURE_ARG(resource);
    NS_ENSURE_ARG(listener);

    nsCOMPtr<nsIURI> resourceURI;
    nsCOMPtr<nsIHttpChannel> channel;
    rv = ChannelFromResource(resource, getter_AddRefs(channel),
                             getter_AddRefs(resourceURI));
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIDOMDocument> requestDoc;
    nsCOMPtr<nsIDOMElement> propfindElt;
    rv = CreatePropfindDocument(resourceURI, getter_AddRefs(requestDoc),
                                getter_AddRefs(propfindElt));
    NS_ENSURE_SUCCESS(rv, rv);

    if (namesOnly) {
        nsCOMPtr<nsIDOMElement> allpropElt;
        rv = NS_WD_AppendElementWithNS(requestDoc, propfindElt,
                                       mDAVNSString, NS_LITERAL_STRING("propname"),
                                       getter_AddRefs(allpropElt));
        NS_ENSURE_SUCCESS(rv, rv);
    } else if (propCount == 0) {
        nsCOMPtr<nsIDOMElement> allpropElt;
        rv = NS_WD_AppendElementWithNS(requestDoc, propfindElt,
                                       mDAVNSString, NS_LITERAL_STRING("allprop"),
                                       getter_AddRefs(allpropElt));
        NS_ENSURE_SUCCESS(rv, rv);
    } else {
        nsCOMPtr<nsIDOMElement> propElt;
        rv = NS_WD_AppendElementWithNS(requestDoc, propfindElt,
                                       mDAVNSString, NS_LITERAL_STRING("prop"),
                                       getter_AddRefs(propElt));
        NS_ENSURE_SUCCESS(rv, rv);

        for (PRUint32 i = 0; i < propCount; i++) {
            nsDependentCString fullpropName(properties[i]);

            // This string math is _ridiculous_.  It better compile to a total of
            // 5 instructions, or I'm ripping it all out and doing my own looping.

            nsACString::const_iterator start, saveStart, end, saveEnd;
            fullpropName.BeginReading(start);
            fullpropName.BeginReading(saveStart);
            fullpropName.EndReading(end);
            fullpropName.EndReading(saveEnd);
            RFindInReadable(NS_LITERAL_CSTRING(" "), start, end);

            if (start == end) {
                nsCAutoString msg(NS_LITERAL_CSTRING("Illegal property name ") +
                                  fullpropName);
                NS_WARNING(msg.get());
                return NS_ERROR_INVALID_ARG;
            }

            if (LOG_ENABLED()) {
                nsACString::const_iterator s = start;
                
                nsCAutoString propNamespace(nsDependentCSubstring(saveStart, s));
                nsCAutoString propName(nsDependentCSubstring(++s, saveEnd));
                
                LOG(("prop ns: '%s', name: '%s'", propNamespace.get(), propName.get()));
            }

            NS_ConvertASCIItoUTF16 propNamespace(nsDependentCSubstring(saveStart, start)),
                propName(nsDependentCSubstring(++start, saveEnd));

            nsCOMPtr<nsIDOMElement> junk;
            rv = NS_WD_AppendElementWithNS(requestDoc, propElt, propNamespace,
                                           propName, getter_AddRefs(junk));
            NS_ENSURE_SUCCESS(rv, rv);
        }
    }

    nsCOMPtr<nsIStreamListener> streamListener = 
        NS_WD_NewPropfindStreamListener(resource, listener, namesOnly);

    if (!streamListener)
        return NS_ERROR_OUT_OF_MEMORY;
    
    nsCOMPtr<nsIDocument> requestBaseDoc = do_QueryInterface(requestDoc);
    return SendPropfindDocumentToChannel(requestBaseDoc, channel,
                                         streamListener, withDepth);
}

NS_IMETHODIMP
nsWebDAVService::GetResourceOptions(nsIWebDAVResource *resource,
                                    nsIWebDAVOperationListener *listener)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWebDAVService::Get(nsIWebDAVResource *resource, nsIStreamListener *listener)
{
    nsresult rv;
    nsCOMPtr<nsIHttpChannel> channel;
    rv = ChannelFromResource(resource, getter_AddRefs(channel));
    if (NS_FAILED(rv))
        return rv;

    return channel->AsyncOpen(listener, channel);
}

NS_IMETHODIMP
nsWebDAVService::GetToOutputStream(nsIWebDAVResource *resource,
                                   nsIOutputStream *stream,
                                   nsIWebDAVOperationListener *listener)
{
    nsCOMPtr<nsIRequestObserver> getObserver;
    nsresult rv;

    rv = NS_WD_NewGetOperationRequestObserver(resource, listener, stream,
                                              getter_AddRefs(getObserver));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIStreamListener> streamListener;
    rv = NS_NewSimpleStreamListener(getter_AddRefs(streamListener),
                                    stream, getObserver);
    NS_ENSURE_SUCCESS(rv, rv);

    return Get(resource, streamListener);
}

NS_IMETHODIMP
nsWebDAVService::Put(nsIWebDAVResource *resource,
                     const nsACString& contentType, nsIInputStream *data,
                     nsIWebDAVOperationListener *listener)
{
    nsCOMPtr<nsIHttpChannel> channel;

    nsresult rv = ChannelFromResource(resource, getter_AddRefs(channel));
    NS_ENSURE_SUCCESS(rv, rv);
    
    nsCOMPtr<nsIUploadChannel> uploadChannel = do_QueryInterface(channel, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = uploadChannel->SetUploadStream(data, contentType, -1);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIStreamListener> streamListener;
    rv = NS_WD_NewOperationStreamListener(resource, listener,
                                          nsIWebDAVOperationListener::PUT,
                                          getter_AddRefs(streamListener));
    NS_ENSURE_SUCCESS(rv, rv);

    channel->SetRequestMethod(NS_LITERAL_CSTRING("PUT"));

    if (LOG_ENABLED()) {
        nsCOMPtr<nsIURI> uri;
        channel->GetURI(getter_AddRefs(uri));
        nsCAutoString spec;
        uri->GetSpec(spec);
        LOG(("PUT starting for %s", spec.get()));
    }

    return channel->AsyncOpen(streamListener, channel);
}

NS_IMETHODIMP
nsWebDAVService::Remove(nsIWebDAVResource *resource,
                        nsIWebDAVOperationListener *listener)
{
    nsCOMPtr<nsIHttpChannel> channel;
    nsresult rv = ChannelFromResource(resource, getter_AddRefs(channel));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIStreamListener> streamListener;
    rv = NS_WD_NewOperationStreamListener(resource, listener,
                                          nsIWebDAVOperationListener::REMOVE,
                                          getter_AddRefs(streamListener));
    NS_ENSURE_SUCCESS(rv, rv);

    channel->SetRequestMethod(NS_LITERAL_CSTRING("DELETE"));

    if (LOG_ENABLED()) {
        nsCOMPtr<nsIURI> uri;
        channel->GetURI(getter_AddRefs(uri));
        nsCAutoString spec;
        uri->GetSpec(spec);
        LOG(("DELETE starting for %s", spec.get()));
    }

    return channel->AsyncOpen(streamListener, channel);
}

NS_IMETHODIMP
nsWebDAVService::MakeCollection(nsIWebDAVResource *resource,
                                nsIWebDAVOperationListener *listener)
{
    nsCOMPtr<nsIHttpChannel> channel;
    nsresult rv = ChannelFromResource(resource, getter_AddRefs(channel));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIStreamListener> streamListener;
    rv = NS_WD_NewOperationStreamListener(resource, listener,
                                          nsIWebDAVOperationListener::MAKE_COLLECTION,
                                          getter_AddRefs(streamListener));
    NS_ENSURE_SUCCESS(rv, rv);

    channel->SetRequestMethod(NS_LITERAL_CSTRING("MKCOL"));

    if (LOG_ENABLED()) {
        nsCOMPtr<nsIURI> uri;
        channel->GetURI(getter_AddRefs(uri));
        nsCAutoString spec;
        uri->GetSpec(spec);
        LOG(("MKCOL starting for %s", spec.get()));
    }

    return channel->AsyncOpen(streamListener, channel);
}

NS_IMETHODIMP
nsWebDAVService::MoveTo(nsIWebDAVResource *resource,
                        const nsACString &destination,
                        nsIWebDAVOperationListener *listener)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWebDAVService::CopyTo(nsIWebDAVResource *resource,
                        const nsACString &destination,
                        nsIWebDAVOperationListener *listener, PRBool recursive,
                        PRBool overwrite)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(nsWebDAVService)

NS_DECL_CLASSINFO(nsWebDAVService)

static const nsModuleComponentInfo components[] =
{
    { "WebDAV Service", NS_WEBDAVSERVICE_CID, NS_WEBDAVSERVICE_CONTRACTID,
      nsWebDAVServiceConstructor,
      NULL, NULL, NULL,
      NS_CI_INTERFACE_GETTER_NAME(nsWebDAVService),
      NULL,
      &NS_CLASSINFO_NAME(nsWebDAVService)
    }
};

NS_IMPL_NSGETMODULE(nsWebDAVService, components)
