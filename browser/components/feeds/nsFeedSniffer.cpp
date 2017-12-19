/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFeedSniffer.h"

#include "mozilla/Unused.h"

#include "nsNetCID.h"
#include "nsXPCOM.h"
#include "nsCOMPtr.h"
#include "nsStringStream.h"

#include "nsBrowserCompsCID.h"

#include "nsICategoryManager.h"
#include "nsIServiceManager.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"

#include "nsIStreamConverterService.h"
#include "nsIStreamConverter.h"

#include "nsIStreamListener.h"

#include "nsIHttpChannel.h"
#include "nsIMIMEHeaderParam.h"

#include "nsMimeTypes.h"
#include "nsIURI.h"
#include <algorithm>

#define TYPE_ATOM "application/atom+xml"
#define TYPE_RSS "application/rss+xml"
#define TYPE_MAYBE_FEED "application/vnd.mozilla.maybe.feed"

#define NS_RDF "http://www.w3.org/1999/02/22-rdf-syntax-ns#"
#define NS_RSS "http://purl.org/rss/1.0/"

#define MAX_BYTES 512u

NS_IMPL_ISUPPORTS(nsFeedSniffer,
                  nsIContentSniffer,
                  nsIStreamListener,
                  nsIRequestObserver)

nsresult
nsFeedSniffer::ConvertEncodedData(nsIRequest* request,
                                  const uint8_t* data,
                                  uint32_t length)
{
  nsresult rv = NS_OK;

 mDecodedData = "";
 nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(request));
  if (!httpChannel)
    return NS_ERROR_NO_INTERFACE;

  nsAutoCString contentEncoding;
  mozilla::Unused << httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("Content-Encoding"),
                                                    contentEncoding);
  if (!contentEncoding.IsEmpty()) {
    nsCOMPtr<nsIStreamConverterService> converterService(do_GetService(NS_STREAMCONVERTERSERVICE_CONTRACTID));
    if (converterService) {
      ToLowerCase(contentEncoding);

      nsCOMPtr<nsIStreamListener> converter;
      rv = converterService->AsyncConvertData(contentEncoding.get(),
                                              "uncompressed", this, nullptr,
                                              getter_AddRefs(converter));
      NS_ENSURE_SUCCESS(rv, rv);

      converter->OnStartRequest(request, nullptr);

      nsCOMPtr<nsIStringInputStream> rawStream =
        do_CreateInstance(NS_STRINGINPUTSTREAM_CONTRACTID);
      if (!rawStream)
        return NS_ERROR_FAILURE;

      rv = rawStream->SetData((const char*)data, length);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = converter->OnDataAvailable(request, nullptr, rawStream, 0, length);
      NS_ENSURE_SUCCESS(rv, rv);

      converter->OnStopRequest(request, nullptr, NS_OK);
    }
  }
  return rv;
}

template<int N>
static bool
StringBeginsWithLowercaseLiteral(nsAString& aString,
                                 const char (&aSubstring)[N])
{
  return StringHead(aString, N).LowerCaseEqualsLiteral(aSubstring);
}

bool
HasAttachmentDisposition(nsIHttpChannel* httpChannel)
{
  if (!httpChannel)
    return false;

  uint32_t disp;
  nsresult rv = httpChannel->GetContentDisposition(&disp);

  if (NS_SUCCEEDED(rv) && disp == nsIChannel::DISPOSITION_ATTACHMENT)
    return true;

  return false;
}

/**
 * @return the first occurrence of a character within a string buffer,
 *         or nullptr if not found
 */
static const char*
FindChar(char c, const char *begin, const char *end)
{
  for (; begin < end; ++begin) {
    if (*begin == c)
      return begin;
  }
  return nullptr;
}

/**
 *
 * Determine if a substring is the "documentElement" in the document.
 *
 * All of our sniffed substrings: <rss, <feed, <rdf:RDF must be the "document"
 * element within the XML DOM, i.e. the root container element. Otherwise,
 * it's possible that someone embedded one of these tags inside a document of
 * another type, e.g. a HTML document, and we don't want to show the preview
 * page if the document isn't actually a feed.
 *
 * @param   start
 *          The beginning of the data being sniffed
 * @param   end
 *          The end of the data being sniffed, right before the substring that
 *          was found.
 * @returns true if the found substring is the documentElement, false
 *          otherwise.
 */
static bool
IsDocumentElement(const char *start, const char* end)
{
  // For every tag in the buffer, check to see if it's a PI, Doctype or
  // comment, our desired substring or something invalid.
  while ( (start = FindChar('<', start, end)) ) {
    ++start;
    if (start >= end)
      return false;

    // Check to see if the character following the '<' is either '?' or '!'
    // (processing instruction or doctype or comment)... these are valid nodes
    // to have in the prologue.
    if (*start != '?' && *start != '!')
      return false;

    // Now advance the iterator until the '>' (We do this because we don't want
    // to sniff indicator substrings that are embedded within other nodes, e.g.
    // comments: <!-- <rdf:RDF .. > -->
    start = FindChar('>', start, end);
    if (!start)
      return false;

    ++start;
  }
  return true;
}

/**
 * Determines whether or not a string exists as the root element in an XML data
 * string buffer.
 * @param   dataString
 *          The data being sniffed
 * @param   substring
 *          The substring being tested for existence and root-ness.
 * @returns true if the substring exists and is the documentElement, false
 *          otherwise.
 */
static bool
ContainsTopLevelSubstring(nsACString& dataString, const char *substring)
{
  nsACString::const_iterator start, end;
  dataString.BeginReading(start);
  dataString.EndReading(end);

  if (!FindInReadable(nsCString(substring), start, end)){
    return false;
  }

  auto offset = start.get() - dataString.Data();

  const char *begin = dataString.BeginReading();

  // Only do the validation when we find the substring.
  return IsDocumentElement(begin, begin + offset);
}

NS_IMETHODIMP
nsFeedSniffer::GetMIMETypeFromContent(nsIRequest* request,
                                      const uint8_t* data,
                                      uint32_t length,
                                      nsACString& sniffedType)
{
  nsCOMPtr<nsIHttpChannel> channel(do_QueryInterface(request));
  if (!channel)
    return NS_ERROR_NO_INTERFACE;

  // Check that this is a GET request, since you can't subscribe to a POST...
  nsAutoCString method;
  mozilla::Unused << channel->GetRequestMethod(method);
  if (!method.EqualsLiteral("GET")) {
    sniffedType.Truncate();
    return NS_OK;
  }

  // We need to find out if this is a load of a view-source document. In this
  // case we do not want to override the content type, since the source display
  // does not need to be converted from feed format to XUL. More importantly,
  // we don't want to change the content type from something
  // nsContentDLF::CreateInstance knows about (e.g. application/xml, text/html
  // etc) to something that only the application fe knows about (maybe.feed)
  // thus deactivating syntax highlighting.
  nsCOMPtr<nsIURI> originalURI;
  channel->GetOriginalURI(getter_AddRefs(originalURI));

  nsAutoCString scheme;
  originalURI->GetScheme(scheme);
  if (scheme.EqualsLiteral("view-source")) {
    sniffedType.Truncate();
    return NS_OK;
  }

  // Check the Content-Type to see if it is set correctly. If it is set to
  // something specific that we think is a reliable indication of a feed, don't
  // bother sniffing since we assume the site maintainer knows what they're
  // doing.
  nsAutoCString contentType;
  channel->GetContentType(contentType);
  bool noSniff = contentType.EqualsLiteral(TYPE_RSS) ||
                   contentType.EqualsLiteral(TYPE_ATOM);

  if (noSniff) {
    // check for an attachment after we have a likely feed.
    if(HasAttachmentDisposition(channel)) {
      sniffedType.Truncate();
      return NS_OK;
    }

    // set the feed header as a response header, since we have good metadata
    // telling us that the feed is supposed to be RSS or Atom
    mozilla::DebugOnly<nsresult> rv =
      channel->SetResponseHeader(NS_LITERAL_CSTRING("X-Moz-Is-Feed"),
                                 NS_LITERAL_CSTRING("1"), false);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    sniffedType.AssignLiteral(TYPE_MAYBE_FEED);
    return NS_OK;
  }

  // Don't sniff arbitrary types.  Limit sniffing to situations that
  // we think can reasonably arise.
  if (!contentType.EqualsLiteral(TEXT_HTML) &&
      !contentType.EqualsLiteral(APPLICATION_OCTET_STREAM) &&
      // Same criterion as XMLHttpRequest.  Should we be checking for "+xml"
      // and check for text/xml and application/xml by hand instead?
      contentType.Find("xml") == -1) {
    sniffedType.Truncate();
    return NS_OK;
  }

  // Now we need to potentially decompress data served with
  // Content-Encoding: gzip
  nsresult rv = ConvertEncodedData(request, data, length);
  if (NS_FAILED(rv))
    return rv;

  // We cap the number of bytes to scan at MAX_BYTES to prevent picking up
  // false positives by accidentally reading document content, e.g. a "how to
  // make a feed" page.
  const char* testData;
  if (mDecodedData.IsEmpty()) {
    testData = (const char*)data;
    length = std::min(length, MAX_BYTES);
  } else {
    testData = mDecodedData.get();
    length = std::min(mDecodedData.Length(), MAX_BYTES);
  }

  // The strategy here is based on that described in:
  // http://blogs.msdn.com/rssteam/articles/PublishersGuide.aspx
  // for interoperarbility purposes.

  // Thus begins the actual sniffing.
  nsDependentCSubstring dataString((const char*)testData, length);

  bool isFeed = false;

  // RSS 0.91/0.92/2.0
  isFeed = ContainsTopLevelSubstring(dataString, "<rss");

  // Atom 1.0
  if (!isFeed)
    isFeed = ContainsTopLevelSubstring(dataString, "<feed");

  // RSS 1.0
  if (!isFeed) {
    bool foundNS_RDF = FindInReadable(NS_LITERAL_CSTRING(NS_RDF), dataString);
    bool foundNS_RSS = FindInReadable(NS_LITERAL_CSTRING(NS_RSS), dataString);
    isFeed = ContainsTopLevelSubstring(dataString, "<rdf:RDF") &&
      foundNS_RDF && foundNS_RSS;
  }

  // If we sniffed a feed, coerce our internal type
  if (isFeed && !HasAttachmentDisposition(channel))
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

nsresult
nsFeedSniffer::AppendSegmentToString(nsIInputStream* inputStream,
                                     void* closure,
                                     const char* rawSegment,
                                     uint32_t toOffset,
                                     uint32_t count,
                                     uint32_t* writeCount)
{
  nsCString* decodedData = static_cast<nsCString*>(closure);
  decodedData->Append(rawSegment, count);
  *writeCount = count;
  return NS_OK;
}

NS_IMETHODIMP
nsFeedSniffer::OnDataAvailable(nsIRequest* request, nsISupports* context,
                               nsIInputStream* stream, uint64_t offset,
                               uint32_t count)
{
  uint32_t read;
  return stream->ReadSegments(AppendSegmentToString, &mDecodedData, count,
                              &read);
}

NS_IMETHODIMP
nsFeedSniffer::OnStopRequest(nsIRequest* request, nsISupports* context,
                             nsresult status)
{
  return NS_OK;
}
