/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BlobURLProtocolHandler_h
#define mozilla_dom_BlobURLProtocolHandler_h

#include "mozilla/Attributes.h"
#include "nsIProtocolHandler.h"
#include "nsIURI.h"
#include "nsCOMPtr.h"
#include "nsIInputStream.h"
#include "nsTArray.h"
#include "nsWeakReference.h"

#define BLOBURI_SCHEME "blob"

class nsIPrincipal;

namespace mozilla {
class BlobURLsReporter;

namespace dom {

class BlobImpl;
class BlobURLRegistrationData;
class ContentParent;
class MediaSource;

class BlobURLProtocolHandler final : public nsIProtocolHandler
                                   , public nsIProtocolHandlerWithDynamicFlags
                                   , public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROTOCOLHANDLER
  NS_DECL_NSIPROTOCOLHANDLERWITHDYNAMICFLAGS

  BlobURLProtocolHandler();

  // Methods for managing uri->object mapping
  // AddDataEntry creates the URI with the given scheme and returns it in aUri
  static nsresult AddDataEntry(mozilla::dom::BlobImpl* aBlobImpl,
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

private:
  ~BlobURLProtocolHandler();

  static void Init();

  // If principal is not null, its origin will be used to generate the URI.
  static nsresult GenerateURIString(nsIPrincipal* aPrincipal,
                                    nsACString &aUri);
};

bool IsBlobURI(nsIURI* aUri);
bool IsMediaSourceURI(nsIURI* aUri);

} // namespace dom
} // namespace mozilla

extern nsresult
NS_GetBlobForBlobURI(nsIURI* aURI, mozilla::dom::BlobImpl** aBlob);

extern nsresult
NS_GetBlobForBlobURISpec(const nsACString& aSpec, mozilla::dom::BlobImpl** aBlob);

extern nsresult
NS_GetSourceForMediaSourceURI(nsIURI* aURI, mozilla::dom::MediaSource** aSource);

#endif /* mozilla_dom_BlobURLProtocolHandler_h */
