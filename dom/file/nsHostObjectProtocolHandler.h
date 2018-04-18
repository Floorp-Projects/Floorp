/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHostObjectProtocolHandler_h
#define nsHostObjectProtocolHandler_h

#include "mozilla/Attributes.h"
#include "nsIProtocolHandler.h"
#include "nsIURI.h"
#include "nsCOMPtr.h"
#include "nsIInputStream.h"
#include "nsTArray.h"
#include "nsWeakReference.h"

#define BLOBURI_SCHEME "blob"
#define FONTTABLEURI_SCHEME "moz-fonttable"
#define RTSPURI_SCHEME "rtsp"

class nsIPrincipal;

namespace mozilla {
class BlobURLsReporter;
class DOMMediaStream;

namespace dom {
class BlobImpl;
class BlobURLRegistrationData;
class ContentParent;
class MediaSource;
} // namespace dom
} // namespace mozilla

class nsHostObjectProtocolHandler : public nsIProtocolHandler
                                  , public nsIProtocolHandlerWithDynamicFlags
                                  , public nsSupportsWeakReference
{
public:
  nsHostObjectProtocolHandler();
  NS_DECL_ISUPPORTS

  // nsIProtocolHandler methods, except for GetScheme which is only defined
  // in subclasses.
  NS_IMETHOD GetDefaultPort(int32_t *aDefaultPort) override;
  NS_IMETHOD GetProtocolFlags(uint32_t *aProtocolFlags) override;
  NS_IMETHOD NewURI(const nsACString & aSpec, const char * aOriginCharset, nsIURI *aBaseURI, nsIURI * *_retval) override;
  NS_IMETHOD NewChannel2(nsIURI *aURI, nsILoadInfo *aLoadinfo, nsIChannel * *_retval) override;
  NS_IMETHOD NewChannel(nsIURI *aURI, nsIChannel * *_retval) override;
  NS_IMETHOD AllowPort(int32_t port, const char * scheme, bool *_retval) override;

  // nsIProtocolHandlerWithDynamicFlags methods
  NS_IMETHOD GetFlagsForURI(nsIURI *aURI, uint32_t *aResult) override;

  // If principal is not null, its origin will be used to generate the URI.
  static nsresult GenerateURIString(const nsACString &aScheme,
                                    nsIPrincipal* aPrincipal,
                                    nsACString &aUri);
  static nsresult GenerateURIStringForBlobURL(nsIPrincipal* aPrincipal,
                                              nsACString &aUri);

  // Methods for managing uri->object mapping
  // AddDataEntry creates the URI with the given scheme and returns it in aUri
  static nsresult AddDataEntry(mozilla::dom::BlobImpl* aBlobImpl,
                               nsIPrincipal* aPrincipal,
                               nsACString& aUri);
  static nsresult AddDataEntry(mozilla::DOMMediaStream* aMediaStream,
                               nsIPrincipal* aPrincipal,
                               nsACString& aUri);
  static nsresult AddDataEntry(mozilla::dom::MediaSource* aMediaSource,
                               nsIPrincipal* aPrincipal,
                               nsACString& aUri);
  // IPC only
  static nsresult AddDataEntry(const nsACString& aURI,
                               nsIPrincipal* aPrincipal,
                               mozilla::dom::BlobImpl* aBlobImpl);

  static void RemoveDataEntry(const nsACString& aUri,
                              bool aBroadcastToOTherProcesses = true);

  static void RemoveDataEntries();

  static bool HasDataEntry(const nsACString& aUri);

  static nsIPrincipal* GetDataEntryPrincipal(const nsACString& aUri);
  static void Traverse(const nsACString& aUri, nsCycleCollectionTraversalCallback& aCallback);

  static bool
  GetAllBlobURLEntries(nsTArray<mozilla::dom::BlobURLRegistrationData>& aRegistrations,
                       mozilla::dom::ContentParent* aCP);

protected:
  virtual ~nsHostObjectProtocolHandler() {}

private:
  static void Init();
};

class nsBlobProtocolHandler : public nsHostObjectProtocolHandler
{
public:
  NS_IMETHOD GetProtocolFlags(uint32_t *aProtocolFlags) override;
  NS_IMETHOD GetScheme(nsACString &result) override;
};

class nsFontTableProtocolHandler : public nsHostObjectProtocolHandler
{
public:
  NS_IMETHOD GetProtocolFlags(uint32_t *aProtocolFlags) override;
  NS_IMETHOD GetScheme(nsACString &result) override;
  NS_IMETHOD NewURI(const nsACString & aSpec,
                    const char *aOriginCharset,
                    nsIURI *aBaseURI,
                    nsIURI **_retval) override;
};

bool IsBlobURI(nsIURI* aUri);
bool IsMediaStreamURI(nsIURI* aUri);
bool IsMediaSourceURI(nsIURI* aUri);

inline bool IsRtspURI(nsIURI* aUri)
{
  bool isRtsp;
  return NS_SUCCEEDED(aUri->SchemeIs(RTSPURI_SCHEME, &isRtsp)) && isRtsp;
}

inline bool IsFontTableURI(nsIURI* aUri)
{
  bool isFont;
  return NS_SUCCEEDED(aUri->SchemeIs(FONTTABLEURI_SCHEME, &isFont)) && isFont;
}

extern nsresult
NS_GetBlobForBlobURI(nsIURI* aURI, mozilla::dom::BlobImpl** aBlob, bool aAlsoIfRevoked = false);

extern nsresult
NS_GetBlobForBlobURISpec(const nsACString& aSpec, mozilla::dom::BlobImpl** aBlob);

extern nsresult
NS_GetStreamForBlobURI(nsIURI* aURI, nsIInputStream** aStream);

extern nsresult
NS_GetStreamForMediaStreamURI(nsIURI* aURI, mozilla::DOMMediaStream** aStream);

extern nsresult
NS_GetSourceForMediaSourceURI(nsIURI* aURI, mozilla::dom::MediaSource** aSource);

#endif /* nsHostObjectProtocolHandler_h */
