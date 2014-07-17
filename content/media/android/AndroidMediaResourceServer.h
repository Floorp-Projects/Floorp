/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(AndroidMediaResourceServer_h_)
#define AndroidMediaResourceServer_h_

#include <map>
#include "nsIServerSocket.h"
#include "MediaResource.h"

namespace mozilla {

class MediaResource;

/*
  AndroidMediaResourceServer instantiates a socket server that understands
  HTTP requests for MediaResource instances. The server runs on an
  automatically selected port and MediaResource instances are registered.
  The registration returns a string URL than can be used to fetch the
  resource. That URL contains a randomly generated path to make it
  difficult for other local applications on the device to guess it.

  The HTTP protocol is limited in that it supports only what the
  Android DataSource implementation uses to fetch media. It
  understands HTTP GET and byte range requests.

  The intent of this class is to be used in Media backends that
  have a system component that does its own network requests. These
  requests are made against this server which then uses standard
  Gecko network requests and media cache usage.

  The AndroidMediaResourceServer can be instantiated on any thread and
  its methods are threadsafe - they can be called on any thread.
  The server socket itself is always run on the main thread and
  this is done by the Start() static method by synchronously
  dispatching to the main thread.
*/
class AndroidMediaResourceServer : public nsRunnable
{
private:
  // Mutex protecting private members of AndroidMediaResourceServer.
  // All member variables below this point in the class definition
  // must acquire the mutex before access.
  mozilla::Mutex mMutex;

  // Server socket used to listen for incoming connections
  nsCOMPtr<nsIServerSocket> mSocket;

  // Mapping between MediaResource URL's to the MediaResource
  // object served at that URL.
  typedef std::map<nsCString,
                  nsRefPtr<mozilla::MediaResource> > ResourceMap;
  ResourceMap mResources;

  // Create a AndroidMediaResourceServer that will listen on an automatically
  // selected port when started. This is private as it should only be
  // called internally from the public 'Start' method.
  AndroidMediaResourceServer();
  NS_IMETHOD Run();

  // Append a random URL path to a string. This is used for creating a
  // unique URl for a resource which helps prevent malicious software
  // running on the same machine as the server from guessing the URL
  // and accessing video data.
  nsresult AppendRandomPath(nsCString& aURL);

public:
  // Create a AndroidMediaResourceServer and start it listening. This call will
  // perform a synchronous request on the main thread.
  static already_AddRefed<AndroidMediaResourceServer> Start();

  // Stops the server from listening and accepting further connections.
  void Stop();

  // Add a MediaResource to be served by this server. Stores the
  // absolute URL that can be used to access the resource in 'aUrl'.
  nsresult AddResource(mozilla::MediaResource* aResource, nsCString& aUrl);

  // Remove a MediaResource so it is no longer served by this server.
  // The URL provided must match exactly that provided by a previous
  // call to "AddResource".
  void RemoveResource(nsCString const& aUrl);

  // Returns the prefix for HTTP requests to the server. This plus
  // the result of AddResource results in an Absolute URL.
  nsCString GetURLPrefix();

  // Returns the resource asociated with a given URL
  already_AddRefed<mozilla::MediaResource> GetResource(nsCString const& aUrl);
};

} // namespace mozilla

#endif
