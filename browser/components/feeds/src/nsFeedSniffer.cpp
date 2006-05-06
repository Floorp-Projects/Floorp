/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Feed Content Sniffer.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger <beng@google.com>
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

#include "nsFeedSniffer.h"

#include "prmem.h"

#include "nsNetCID.h"
#include "nsXPCOM.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsStringStream.h"

#include "nsBrowserCompsCID.h"

#include "nsICategoryManager.h"
#include "nsIServiceManager.h"

#include "nsIStreamConverterService.h"
#include "nsIStreamConverter.h"

#include "nsIStreamListener.h"

#include "nsIHttpChannel.h"

#define TYPE_ATOM "application/atom+xml"
#define TYPE_RSS "application/rss+xml"
#define TYPE_MAYBE_FEED "application/vnd.mozilla.maybe.feed"

#define NS_RDF "http://www.w3.org/1999/02/22-rdf-syntax-ns#"
#define NS_RSS "http://purl.org/rss/1.0/"

#define MAX_BYTES 512

NS_IMPL_ISUPPORTS2(nsFeedSniffer, nsIContentSniffer, nsIStreamListener)

nsresult
nsFeedSniffer::ConvertEncodedData(nsIRequest* request,
                                  const PRUint8* data,
                                  PRUint32 length)
{
  nsresult rv = NS_OK;

 mDecodedData = "";
 nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(request));
  if (!httpChannel)
    return NS_ERROR_NO_INTERFACE;

  nsCAutoString contentEncoding;
  httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("Content-Encoding"), 
                                 contentEncoding);
  if (!contentEncoding.IsEmpty()) {
    nsCOMPtr<nsIStreamConverterService> converterService(do_GetService(NS_STREAMCONVERTERSERVICE_CONTRACTID));
    if (converterService) {
      ToLowerCase(contentEncoding);

      nsCOMPtr<nsIStreamListener> converter;
      rv = converterService->AsyncConvertData(contentEncoding.get(), 
                                              "uncompressed", this, nsnull, 
                                              getter_AddRefs(converter));
      NS_ENSURE_SUCCESS(rv, rv);

      converter->OnStartRequest(request, nsnull);

      nsCOMPtr<nsIInputStream> rawStream;
      rv = NS_NewByteInputStream(getter_AddRefs(rawStream), 
                                 (const char*)data, length);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = converter->OnDataAvailable(request, nsnull, rawStream, 0, length);
      NS_ENSURE_SUCCESS(rv, rv);

      converter->OnStopRequest(request, nsnull, NS_OK);
    }
  }
  return rv;
}

NS_IMETHODIMP
nsFeedSniffer::GetMIMETypeFromContent(nsIRequest* request, 
                                      const PRUint8* data, 
                                      PRUint32 length, 
                                      nsACString& sniffedType)
{
  nsCOMPtr<nsIChannel> channel(do_QueryInterface(request));

  // We need to find out if this is a load of a view-source document. In this
  // case we do not want to override the content type, since the source display
  // does not need to be converted from feed format to XUL. More importantly, 
  // we don't want to change the content type from something 
  // nsContentDLF::CreateInstance knows about (e.g. application/xml, text/html 
  // etc) to something that only the application fe knows about (maybe.feed) 
  // thus deactivating syntax highlighting.
  nsCOMPtr<nsIURI> originalURI;
  channel->GetOriginalURI(getter_AddRefs(originalURI));

  nsCAutoString scheme;
  originalURI->GetScheme(scheme);
  if (scheme.EqualsLiteral("view-source")) {
    sniffedType.Truncate();
    return NS_OK;
  }

  // Check the Content-Type to see if it is set correctly. If it is set to 
  // something specific that we think is a reliable indication of a feed, don't
  // bother sniffing since we assume the site maintainer knows what they're 
  // doing. 
  nsCAutoString contentType;
  channel->GetContentType(contentType);
  if (contentType.EqualsLiteral(TYPE_RSS) ||
      contentType.EqualsLiteral(TYPE_ATOM)) {
    sniffedType.AssignLiteral(TYPE_MAYBE_FEED);
    return NS_OK;
  }

  // Now we need to potentially decompress data served with 
  // Content-Encoding: gzip
  nsresult rv = ConvertEncodedData(request, data, length);
  if (NS_FAILED(rv))
    return rv;
  
  const char* testData = 
    mDecodedData.IsEmpty() ? (const char*)data : mDecodedData.get();

  // The strategy here is based on that described in:
  // http://blogs.msdn.com/rssteam/articles/PublishersGuide.aspx
  // for interoperarbility purposes.

  // We cap the number of bytes to scan at MAX_BYTES to prevent picking up 
  // false positives by accidentally reading document content, e.g. a "how to
  // make a feed" page.
  if (length > MAX_BYTES)
    length = MAX_BYTES;

  // Thus begins the actual sniffing.
  nsDependentCSubstring dataString((const char*)testData, 
                                   (const char*)testData + length);
  nsACString::const_iterator start_iter, end_iter;

  PRBool isFeed = PR_FALSE;

  // RSS 0.91/0.92/2.0
  dataString.BeginReading(start_iter);
  dataString.EndReading(end_iter);

  isFeed = FindInReadable(NS_LITERAL_CSTRING("<rss"), start_iter, end_iter);

  // Atom 1.0
  if (!isFeed) {
    dataString.BeginReading(start_iter);
    dataString.EndReading(end_iter);
    isFeed = FindInReadable(NS_LITERAL_CSTRING("<feed"), start_iter, end_iter);
  }

  // RSS 1.0
  if (!isFeed) {
    dataString.BeginReading(start_iter);
    dataString.EndReading(end_iter);
    isFeed = FindInReadable(NS_LITERAL_CSTRING("<rdf:RDF"), start_iter, end_iter);
    if (isFeed) {
      dataString.BeginReading(start_iter);
      dataString.EndReading(end_iter);
      isFeed = FindInReadable(NS_LITERAL_CSTRING(NS_RDF), start_iter, end_iter);
      if (isFeed) {
        dataString.BeginReading(start_iter);
        dataString.EndReading(end_iter);
        isFeed = FindInReadable(NS_LITERAL_CSTRING(NS_RSS), start_iter, end_iter);
      }
    }
  }

  // If we sniffed a feed, coerce our internal type
  if (isFeed)
    sniffedType.AssignLiteral(TYPE_MAYBE_FEED);
  else
    sniffedType.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsFeedSniffer::OnStartRequest(nsIRequest* request, nsISupports* context)
{
  return NS_OK;
}

NS_METHOD
nsFeedSniffer::AppendSegmentToString(nsIInputStream* inputStream,
                                     void* closure,
                                     const char* rawSegment,
                                     PRUint32 toOffset,
                                     PRUint32 count,
                                     PRUint32* writeCount)
{
  nsCString* decodedData = NS_STATIC_CAST(nsCString*, closure);
  decodedData->Append(rawSegment, count);
  *writeCount = count;
  return NS_OK;
}

NS_IMETHODIMP
nsFeedSniffer::OnDataAvailable(nsIRequest* request, nsISupports* context,
                               nsIInputStream* stream, PRUint32 offset, 
                               PRUint32 count)
{
  PRUint32 read;
  return stream->ReadSegments(AppendSegmentToString, &mDecodedData, count, 
                              &read);
}

NS_IMETHODIMP
nsFeedSniffer::OnStopRequest(nsIRequest* request, nsISupports* context, 
                             nsresult status)
{
  return NS_OK; 
}

NS_METHOD
nsFeedSniffer::Register(nsIComponentManager *compMgr, nsIFile *path, 
                        const char *registryLocation,
                        const char *componentType, 
                        const nsModuleComponentInfo *info)
{
  nsresult rv;
  nsCOMPtr<nsICategoryManager> catman = do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) 
    return rv;

  return catman->AddCategoryEntry(NS_CONTENT_SNIFFER_CATEGORY, "Feed Sniffer", 
                                  NS_FEEDSNIFFER_CONTRACTID, PR_TRUE, PR_TRUE, 
                                  nsnull);
}
