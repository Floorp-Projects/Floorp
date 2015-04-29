/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/Assertions.h"
#include "mozilla/Base64.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "nsThreadUtils.h"
#include "nsIServiceManager.h"
#include "nsISocketTransport.h"
#include "nsIOutputStream.h"
#include "nsIInputStream.h"
#include "nsIRandomGenerator.h"
#include "nsReadLine.h"
#include "nsNetCID.h"
#include "VideoUtils.h"
#include "MediaResource.h"
#include "AndroidMediaResourceServer.h"

#if defined(_MSC_VER)
#define strtoll _strtoi64
#if _MSC_VER < 1900
#define snprintf _snprintf_s
#endif
#endif

using namespace mozilla;

/*
  ReadCRLF is a variant of NS_ReadLine from nsReadLine.h that deals
  with the carriage return/line feed requirements of HTTP requests.
*/
template<typename CharT, class StreamType, class StringType>
nsresult
ReadCRLF (StreamType* aStream, nsLineBuffer<CharT> * aBuffer,
          StringType & aLine, bool *aMore)
{
  // eollast is true if the last character in the buffer is a '\r',
  // signaling a potential '\r\n' sequence split between reads.
  bool eollast = false;

  aLine.Truncate();

  while (1) { // will be returning out of this loop on eol or eof
    if (aBuffer->start == aBuffer->end) { // buffer is empty.  Read into it.
      uint32_t bytesRead;
      nsresult rv = aStream->Read(aBuffer->buf, kLineBufferSize, &bytesRead);
      if (NS_FAILED(rv) || bytesRead == 0) {
        *aMore = false;
        return rv;
      }
      aBuffer->start = aBuffer->buf;
      aBuffer->end = aBuffer->buf + bytesRead;
      *(aBuffer->end) = '\0';
    }

    /*
     * Walk the buffer looking for an end-of-line.
     * There are 4 cases to consider:
     *  1. the CR char is the last char in the buffer
     *  2. the CRLF sequence are the last characters in the buffer
     *  3. the CRLF sequence + one or more chars at the end of the buffer
     *      we need at least one char after the first CRLF sequence to
     *      set |aMore| correctly.
     *  4. The LF character is the first char in the buffer when eollast is
     *      true.
     */
    CharT* current = aBuffer->start;
    if (eollast) { // Case 4
      if (*current == '\n') {
        aBuffer->start = ++current;
        *aMore = true;
        return NS_OK;
      }
      else {
        eollast = false;
        aLine.Append('\r');
      }
    }
    // Cases 2 and 3
    for ( ; current < aBuffer->end-1; ++current) {
      if (*current == '\r' && *(current+1) == '\n') {
        *current++ = '\0';
        *current++ = '\0';
        aLine.Append(aBuffer->start);
        aBuffer->start = current;
        *aMore = true;
        return NS_OK;
      }
    }
    // Case 1
    if (*current == '\r') {
      eollast = true;
      *current++ = '\0';
    }

    aLine.Append(aBuffer->start);
    aBuffer->start = aBuffer->end; // mark the buffer empty
  }
}

// Each client HTTP request results in a thread being spawned to process it.
// That thread has a single event dispatched to it which handles the HTTP
// protocol. It parses the headers and forwards data from the MediaResource
// associated with the URL back to client. When the request is complete it will
// shutdown the thread.
class ServeResourceEvent : public nsRunnable {
private:
  // Reading from this reads the data sent from the client.
  nsCOMPtr<nsIInputStream> mInput;

  // Writing to this sends data to the client.
  nsCOMPtr<nsIOutputStream> mOutput;

  // The AndroidMediaResourceServer that owns the MediaResource instances
  // served. This is used to lookup the MediaResource from the URL.
  nsRefPtr<AndroidMediaResourceServer> mServer;

  // Write 'aBufferLength' bytes from 'aBuffer' to 'mOutput'. This
  // method ensures all the data is written by checking the number
  // of bytes returned from the output streams 'Write' method and
  // looping until done.
  nsresult WriteAll(char const* aBuffer, int32_t aBufferLength);

public:
  ServeResourceEvent(nsIInputStream* aInput, nsIOutputStream* aOutput,
                     AndroidMediaResourceServer* aServer)
    : mInput(aInput), mOutput(aOutput), mServer(aServer) {}

  // This method runs on the thread and exits when it has completed the
  // HTTP request.
  NS_IMETHOD Run();

  // Given the first line of an HTTP request, parse the URL requested and
  // return the MediaResource for that URL.
  already_AddRefed<MediaResource> GetMediaResource(nsCString const& aHTTPRequest);

  // Gracefully shutdown the thread and cleanup resources
  void Shutdown();
};

nsresult
ServeResourceEvent::WriteAll(char const* aBuffer, int32_t aBufferLength)
{
  while (aBufferLength > 0) {
    uint32_t written = 0;
    nsresult rv = mOutput->Write(aBuffer, aBufferLength, &written);
    if (NS_FAILED (rv)) return rv;

    aBufferLength -= written;
    aBuffer += written;
  }

  return NS_OK;
}

already_AddRefed<MediaResource>
ServeResourceEvent::GetMediaResource(nsCString const& aHTTPRequest)
{
  // Check that the HTTP method is GET
  const char* HTTP_METHOD = "GET ";
  if (strncmp(aHTTPRequest.get(), HTTP_METHOD, strlen(HTTP_METHOD)) != 0) {
    return nullptr;
  }

  const char* url_start = strchr(aHTTPRequest.get(), ' ');
  if (!url_start) {
    return nullptr;
  }

  const char* url_end = strrchr(++url_start, ' ');
  if (!url_end) {
    return nullptr;
  }

  // The path extracted from the HTTP request is used as a key in hash
  // table. It is not related to retrieving data from the filesystem so
  // we don't need to do any sanity checking on ".." paths and similar
  // exploits.
  nsCString relative(url_start, url_end - url_start);
  nsRefPtr<MediaResource> resource =
    mServer->GetResource(mServer->GetURLPrefix() + relative);
  return resource.forget();
}

NS_IMETHODIMP
ServeResourceEvent::Run() {
  bool more = false; // Are there HTTP headers to read after the first line
  nsCString line;    // Contains the current line read from input stream
  nsLineBuffer<char>* buffer = new nsLineBuffer<char>();
  nsresult rv = ReadCRLF(mInput.get(), buffer, line, &more);
  if (NS_FAILED(rv)) { Shutdown(); return rv; }

  // First line contains the HTTP GET request. Extract the URL and obtain
  // the MediaResource for it.
  nsRefPtr<MediaResource> resource = GetMediaResource(line);
  if (!resource) {
    const char* response_404 = "HTTP/1.1 404 Not Found\r\n"
                               "Content-Length: 0\r\n\r\n";
    rv = WriteAll(response_404, strlen(response_404));
    Shutdown();
    return rv;
  }

  // Offset in bytes to start reading from resource.
  // This is zero by default but can be set to another starting value if
  // this HTTP request includes a byte range request header.
  int64_t start = 0;

  // Keep reading lines until we get a zero length line, which is the HTTP
  // protocol's way of signifying the end of headers and start of body, or
  // until we have no more data to read.
  while (more && line.Length() > 0) {
    rv = ReadCRLF(mInput.get(), buffer, line, &more);
    if (NS_FAILED(rv)) { Shutdown(); return rv; }

    // Look for a byte range request header. If there is one, set the
    // media resource offset to start from to that requested. Here we
    // only check for the range request format used by Android rather
    // than implementing all possibilities in the HTTP specification.
    // That is, the range request is of the form:
    //   Range: bytes=nnnn-
    // Were 'nnnn' is an integer number.
    // The end of the range is not checked, instead we return up to
    // the end of the resource and the client is informed of this via
    // the content-range header.
    NS_NAMED_LITERAL_CSTRING(byteRange, "Range: bytes=");
    const char* s = strstr(line.get(), byteRange.get());
    if (s) {
      start = strtoll(s+byteRange.Length(), nullptr, 10);

      // Clamp 'start' to be between 0 and the resource length.
      start = std::max(int64_t(0), std::min(resource->GetLength(), start));
    }
  }

  // HTTP response to use if this is a non byte range request
  const char* response_normal = "HTTP/1.1 200 OK\r\n";

  // HTTP response to use if this is a byte range request
  const char* response_range = "HTTP/1.1 206 Partial Content\r\n";

  // End of HTTP reponse headers is indicated by an empty line.
  const char* response_end = "\r\n";

  // If the request was a byte range request, we need to read from the
  // requested offset. If the resource is non-seekable, or the seek
  // fails, then the start offset is set back to zero. This results in all
  // HTTP response data being as if the byte range request was not made.
  if (start > 0 && !resource->IsTransportSeekable()) {
    start = 0;
  }

  const char* response_line = start > 0 ?
                                response_range :
                                response_normal;
  rv = WriteAll(response_line, strlen(response_line));
  if (NS_FAILED(rv)) { Shutdown(); return NS_OK; }

  // Buffer used for reading from the input stream and writing to
  // the output stream. The buffer size should be big enough for the
  // HTTP response headers sent below. A static_assert ensures
  // this where the buffer is used.
  const int buffer_size = 32768;
  nsAutoArrayPtr<char> b(new char[buffer_size]);

  // If we know the length of the resource, send a Content-Length header.
  int64_t contentlength = resource->GetLength() - start;
  if (contentlength > 0) {
    static_assert (buffer_size > 1024,
                   "buffer_size must be large enough "
                   "to hold response headers");
    snprintf(b, buffer_size, "Content-Length: %" PRId64 "\r\n", contentlength);
    rv = WriteAll(b, strlen(b));
    if (NS_FAILED(rv)) { Shutdown(); return NS_OK; }
  }

  // If the request was a byte range request, respond with a Content-Range
  // header which details the extent of the data returned.
  if (start > 0) {
    static_assert (buffer_size > 1024,
                   "buffer_size must be large enough "
                   "to hold response headers");
    snprintf(b, buffer_size, "Content-Range: "
             "bytes %" PRId64 "-%" PRId64 "/%" PRId64 "\r\n",
             start, resource->GetLength() - 1, resource->GetLength());
    rv = WriteAll(b, strlen(b));
    if (NS_FAILED(rv)) { Shutdown(); return NS_OK; }
  }

  rv = WriteAll(response_end, strlen(response_end));
  if (NS_FAILED(rv)) { Shutdown(); return NS_OK; }

  rv = mOutput->Flush();
  if (NS_FAILED(rv)) { Shutdown(); return NS_OK; }

  // Read data from media resource
  uint32_t bytesRead = 0; // Number of bytes read/written to streams
  rv = resource->ReadAt(start, b, buffer_size, &bytesRead);
  while (NS_SUCCEEDED(rv) && bytesRead != 0) {
    // Keep track of what we think the starting position for the next read
    // is. This is used in subsequent ReadAt calls to ensure we are reading
    // from the correct offset in the case where another thread is reading
    // from th same MediaResource.
    start += bytesRead;

    // Write data obtained from media resource to output stream
    rv = WriteAll(b, bytesRead);
    if (NS_FAILED (rv)) break;

    rv = resource->ReadAt(start, b, 32768, &bytesRead);
  }

  Shutdown();
  return NS_OK;
}

void
ServeResourceEvent::Shutdown()
{
  // Cleanup resources and exit.
  mInput->Close();
  mOutput->Close();

  // To shutdown the current thread we need to first exit this event.
  // The Shutdown event below is posted to the main thread to do this.
  nsCOMPtr<nsIRunnable> event = new ShutdownThreadEvent(NS_GetCurrentThread());
  NS_DispatchToMainThread(event);
}

/*
  This is the listener attached to the server socket. When an HTTP
  request is made by the client the OnSocketAccepted method is
  called. This method will spawn a thread to process the request.
  The thread receives a single event which does the parsing of
  the HTTP request and forwarding the data from the MediaResource
  to the output stream of the request.

  The MediaResource used for providing the request data is obtained
  from the AndroidMediaResourceServer that created this listener, using the
  URL the client requested.
*/
class ResourceSocketListener : public nsIServerSocketListener
{
public:
  // The AndroidMediaResourceServer used to look up the MediaResource
  // on requests.
  nsRefPtr<AndroidMediaResourceServer> mServer;

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISERVERSOCKETLISTENER

  ResourceSocketListener(AndroidMediaResourceServer* aServer) :
    mServer(aServer)
  {
  }

private:
  virtual ~ResourceSocketListener() { }
};

NS_IMPL_ISUPPORTS(ResourceSocketListener, nsIServerSocketListener)

NS_IMETHODIMP
ResourceSocketListener::OnSocketAccepted(nsIServerSocket* aServ,
                                         nsISocketTransport* aTrans)
{
  nsCOMPtr<nsIInputStream> input;
  nsCOMPtr<nsIOutputStream> output;
  nsresult rv;

  rv = aTrans->OpenInputStream(nsITransport::OPEN_BLOCKING, 0, 0, getter_AddRefs(input));
  if (NS_FAILED(rv)) return rv;

  rv = aTrans->OpenOutputStream(nsITransport::OPEN_BLOCKING, 0, 0, getter_AddRefs(output));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIThread> thread;
  rv = NS_NewThread(getter_AddRefs(thread));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIRunnable> event = new ServeResourceEvent(input.get(), output.get(), mServer);
  return thread->Dispatch(event, NS_DISPATCH_NORMAL);
}

NS_IMETHODIMP
ResourceSocketListener::OnStopListening(nsIServerSocket* aServ, nsresult aStatus)
{
  return NS_OK;
}

AndroidMediaResourceServer::AndroidMediaResourceServer() :
  mMutex("AndroidMediaResourceServer")
{
}

NS_IMETHODIMP
AndroidMediaResourceServer::Run()
{
  MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mMutex);

  nsresult rv;
  mSocket = do_CreateInstance(NS_SERVERSOCKET_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = mSocket->InitSpecialConnection(-1,
                                      nsIServerSocket::LoopbackOnly
                                      | nsIServerSocket::KeepWhenOffline,
                                      -1);
  if (NS_FAILED(rv)) return rv;

  rv = mSocket->AsyncListen(new ResourceSocketListener(this));
  if (NS_FAILED(rv)) return rv;

  return NS_OK;
}

/* static */
already_AddRefed<AndroidMediaResourceServer>
AndroidMediaResourceServer::Start()
{
  MOZ_ASSERT(NS_IsMainThread());
  nsRefPtr<AndroidMediaResourceServer> server = new AndroidMediaResourceServer();
  server->Run();
  return server.forget();
}

void
AndroidMediaResourceServer::Stop()
{
  MutexAutoLock lock(mMutex);
  mSocket->Close();
  mSocket = nullptr;
}

nsresult
AndroidMediaResourceServer::AppendRandomPath(nsCString& aUrl)
{
  // Use a cryptographic quality PRNG to generate raw random bytes
  // and convert that to a base64 string for use as an URL path. This
  // is based on code from nsExternalAppHandler::SetUpTempFile.
  nsresult rv;
  nsAutoCString salt;
  rv = GenerateRandomPathName(salt, 16);
  if (NS_FAILED(rv)) return rv;
  aUrl += "/";
  aUrl += salt;
  return NS_OK;
}

nsresult
AndroidMediaResourceServer::AddResource(mozilla::MediaResource* aResource, nsCString& aUrl)
{
  nsCString url = GetURLPrefix();
  nsresult rv = AppendRandomPath(url);
  if (NS_FAILED (rv)) return rv;

  {
    MutexAutoLock lock(mMutex);

    // Adding a resource URL that already exists is considered an error.
    if (mResources.find(aUrl) != mResources.end()) return NS_ERROR_FAILURE;
    mResources[url] = aResource;
  }

  aUrl = url;

  return NS_OK;
}

void
AndroidMediaResourceServer::RemoveResource(nsCString const& aUrl)
{
  MutexAutoLock lock(mMutex);
  mResources.erase(aUrl);
}

nsCString
AndroidMediaResourceServer::GetURLPrefix()
{
  MutexAutoLock lock(mMutex);

  int32_t port = 0;
  nsresult rv = mSocket->GetPort(&port);
  if (NS_FAILED (rv) || port < 0) {
    return nsCString("");
  }

  char buffer[256];
  snprintf(buffer, sizeof(buffer), "http://127.0.0.1:%d", port >= 0 ? port : 0);
  return nsCString(buffer);
}

already_AddRefed<MediaResource>
AndroidMediaResourceServer::GetResource(nsCString const& aUrl)
{
  MutexAutoLock lock(mMutex);
  ResourceMap::const_iterator it = mResources.find(aUrl);
  if (it == mResources.end()) return nullptr;

  nsRefPtr<MediaResource> resource = it->second;
  return resource.forget();
}
