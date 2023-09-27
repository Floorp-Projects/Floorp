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
#include "nsTArray.h"
#include "nsWeakReference.h"
#include <functional>

#define BLOBURI_SCHEME "blob"

class nsIPrincipal;

namespace mozilla {
class BlobURLsReporter;
class OriginAttributes;
template <class T>
class Maybe;

namespace dom {

class BlobImpl;
class BlobURLRegistrationData;
class ContentParent;
class MediaSource;

class BlobURLProtocolHandler final : public nsIProtocolHandler,
                                     public nsSupportsWeakReference {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROTOCOLHANDLER

  BlobURLProtocolHandler();

  static nsresult CreateNewURI(const nsACString& aSpec, const char* aCharset,
                               nsIURI* aBaseURI, nsIURI** result);

  // Methods for managing uri->object mapping
  // AddDataEntry creates the URI with the given scheme and returns it in aUri
  static nsresult AddDataEntry(BlobImpl*, nsIPrincipal*,
                               const nsCString& aPartitionKey,
                               nsACString& aUri);
  static nsresult AddDataEntry(MediaSource*, nsIPrincipal*,
                               const nsCString& aPartitionKey,
                               nsACString& aUri);
  // IPC only
  static void AddDataEntry(const nsACString& aURI, nsIPrincipal* aPrincipal,
                           const nsCString& aPartitionKey, BlobImpl* aBlobImpl);

  // These methods revoke a blobURL. Because some operations could still be in
  // progress, the revoking consists in marking the blobURL as revoked and in
  // removing it after RELEASING_TIMER milliseconds.
  static void RemoveDataEntry(const nsACString& aUri,
                              bool aBroadcastToOTherProcesses = true);
  // Returns true if the entry was allowed to be removed.
  static bool RemoveDataEntry(const nsACString& aUri, nsIPrincipal* aPrincipal,
                              const nsCString& aPartitionKey);

  static void RemoveDataEntries();

  static bool HasDataEntry(const nsACString& aUri);

  static bool GetDataEntry(const nsACString& aUri, BlobImpl** aBlobImpl,
                           nsIPrincipal* aLoadingPrincipal,
                           nsIPrincipal* aTriggeringPrincipal,
                           const OriginAttributes& aOriginAttributes,
                           uint64_t aInnerWindowId,
                           const nsCString& aPartitionKey,
                           bool aAlsoIfRevoked = false);

  static void Traverse(const nsACString& aUri,
                       nsCycleCollectionTraversalCallback& aCallback);

  // Main-thread only method to invoke a helper function that gets called for
  // every known and recently revoked Blob URL. The helper function should
  // return true to keep going or false to stop enumerating (presumably because
  // of an unexpected XPCOM or IPC error). This method returns false if already
  // shutdown or if the helper method returns false, true otherwise.
  static bool ForEachBlobURL(
      std::function<bool(BlobImpl*, nsIPrincipal*, const nsCString&,
                         const nsACString&, bool aRevoked)>&& aCb);

  // This method returns false if aURI is not a known BlobURL. Otherwise it
  // returns true.
  //
  // When true is returned, the aPrincipal out param is meaningful.  It gets
  // set to the principal that a channel loaded from the blob would get if
  // the blob is not already revoked and to a NullPrincipal if the blob is
  // revoked.
  //
  // This means that for a revoked blob URL this method may either return
  // false or return true and hand out a NullPrincipal in aPrincipal,
  // depending on whether the "remove it from the hashtable" timer has
  // fired.  See RemoveDataEntry().
  static bool GetBlobURLPrincipal(nsIURI* aURI, nsIPrincipal** aPrincipal);

  // Check if metadata about Blob URLs created with this principal should be
  // broadcast into every content process. This is currently the case for
  // extension blob URLs and system principal blob URLs, as they can be loaded
  // by system code and content scripts respectively.
  static bool IsBlobURLBroadcastPrincipal(nsIPrincipal* aPrincipal);

 private:
  ~BlobURLProtocolHandler();

  static void Init();

  // If principal is not null, its origin will be used to generate the URI.
  static nsresult GenerateURIString(nsIPrincipal* aPrincipal, nsACString& aUri);
};

bool IsBlobURI(nsIURI* aUri);
bool IsMediaSourceURI(nsIURI* aUri);

// Return true if inner scheme of blobURL is http or https, false otherwise.
bool BlobURLSchemeIsHTTPOrHTTPS(const nsACString& aUri);

}  // namespace dom
}  // namespace mozilla

extern nsresult NS_GetBlobForBlobURI(nsIURI* aURI,
                                     mozilla::dom::BlobImpl** aBlob);

extern nsresult NS_GetBlobForBlobURISpec(const nsACString& aSpec,
                                         mozilla::dom::BlobImpl** aBlob,
                                         bool aAlsoIfRevoked = false);

extern nsresult NS_SetChannelContentRangeForBlobURI(nsIChannel* aChannel,
                                                    nsIURI* aURI,
                                                    nsACString& aRangeHeader);

extern nsresult NS_GetSourceForMediaSourceURI(
    nsIURI* aURI, mozilla::dom::MediaSource** aSource);

#endif /* mozilla_dom_BlobURLProtocolHandler_h */
