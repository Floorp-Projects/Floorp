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

class BlobURLProtocolHandler final : public nsIProtocolHandler,
                                     public nsIProtocolHandlerWithDynamicFlags,
                                     public nsSupportsWeakReference {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROTOCOLHANDLER
  NS_DECL_NSIPROTOCOLHANDLERWITHDYNAMICFLAGS

  BlobURLProtocolHandler();

  static nsresult CreateNewURI(const nsACString& aSpec, const char* aCharset,
                               nsIURI* aBaseURI, nsIURI** result);

  // Methods for managing uri->object mapping
  // AddDataEntry creates the URI with the given scheme and returns it in aUri
  static nsresult AddDataEntry(mozilla::dom::BlobImpl* aBlobImpl,
                               nsIPrincipal* aPrincipal, nsACString& aUri);
  static nsresult AddDataEntry(mozilla::dom::MediaSource* aMediaSource,
                               nsIPrincipal* aPrincipal, nsACString& aUri);
  // IPC only
  static nsresult AddDataEntry(const nsACString& aURI, nsIPrincipal* aPrincipal,
                               mozilla::dom::BlobImpl* aBlobImpl);

  // This method revokes a blobURL. Because some operations could still be in
  // progress, the revoking consists in marking the blobURL as revoked and in
  // removing it after RELEASING_TIMER milliseconds.
  static void RemoveDataEntry(const nsACString& aUri,
                              bool aBroadcastToOTherProcesses = true);

  static void RemoveDataEntries();

  static bool HasDataEntry(const nsACString& aUri);

  static nsIPrincipal* GetDataEntryPrincipal(const nsACString& aUri);
  static void Traverse(const nsACString& aUri,
                       nsCycleCollectionTraversalCallback& aCallback);

  static bool GetAllBlobURLEntries(
      nsTArray<mozilla::dom::BlobURLRegistrationData>& aRegistrations,
      mozilla::dom::ContentParent* aCP);

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

 private:
  ~BlobURLProtocolHandler();

  static void Init();

  // If principal is not null, its origin will be used to generate the URI.
  static nsresult GenerateURIString(nsIPrincipal* aPrincipal, nsACString& aUri);
};

bool IsBlobURI(nsIURI* aUri);
bool IsMediaSourceURI(nsIURI* aUri);

}  // namespace dom
}  // namespace mozilla

extern nsresult NS_GetBlobForBlobURI(nsIURI* aURI,
                                     mozilla::dom::BlobImpl** aBlob);

extern nsresult NS_GetBlobForBlobURISpec(const nsACString& aSpec,
                                         mozilla::dom::BlobImpl** aBlob);

extern nsresult NS_GetSourceForMediaSourceURI(
    nsIURI* aURI, mozilla::dom::MediaSource** aSource);

#endif /* mozilla_dom_BlobURLProtocolHandler_h */
