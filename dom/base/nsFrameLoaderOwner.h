/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFrameLoaderOwner_h_
#define nsFrameLoaderOwner_h_

#include <functional>
#include "nsFrameLoader.h"
#include "nsISupports.h"

namespace mozilla {
class ErrorResult;
namespace dom {
class BrowsingContext;
class BrowsingContextGroup;
class BrowserBridgeChild;
class ContentParent;
class Element;
struct RemotenessOptions;
struct NavigationIsolationOptions;
}  // namespace dom
}  // namespace mozilla

// IID for the FrameLoaderOwner interface
#define NS_FRAMELOADEROWNER_IID                      \
  {                                                  \
    0x1b4fd25c, 0x2e57, 0x11e9, {                    \
      0x9e, 0x5a, 0x5b, 0x86, 0xe9, 0x89, 0xa5, 0xc0 \
    }                                                \
  }

// Mixin that handles ownership of nsFrameLoader for Frame elements
// (XULFrameElement, HTMLI/FrameElement, etc...). Manages information when doing
// FrameLoader swaps.
//
// This class is considered an XPCOM mixin. This means that while we inherit
// from ISupports in order to be QI'able, we expect the classes that inherit
// nsFrameLoaderOwner to actually implement ISupports for us.
class nsFrameLoaderOwner : public nsISupports {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_FRAMELOADEROWNER_IID)

  nsFrameLoaderOwner() = default;
  already_AddRefed<nsFrameLoader> GetFrameLoader();
  void SetFrameLoader(nsFrameLoader* aNewFrameLoader);

  mozilla::dom::BrowsingContext* GetBrowsingContext();
  mozilla::dom::BrowsingContext* GetExtantBrowsingContext();

  // Destroy (if it exists) and recreate our frameloader, based on new
  // remoteness requirements.
  //
  // This method is called by frontend code when it wants to perform a
  // remoteness update, and allows for behaviour such as preserving
  // BrowsingContexts across process switches during navigation.
  //
  // See the WebIDL definition for more details.
  void ChangeRemoteness(const mozilla::dom::RemotenessOptions& aOptions,
                        mozilla::ErrorResult& rv);

  // Like `ChangeRemoteness` but switches to an already-created
  // `BrowserBridgeChild`. This method is used when performing remote subframe
  // process switches.
  void ChangeRemotenessWithBridge(mozilla::dom::BrowserBridgeChild* aBridge,
                                  mozilla::ErrorResult& rv);

  // Like `ChangeRemoteness`, but switches into an already-created
  // `ContentParent`. This method is used when performing toplevel process
  // switches. If `aContentParent` is nullptr, switches into the parent process.
  //
  // If `aReplaceBrowsingContext` is set, BrowsingContext preservation will be
  // disabled for this process switch.
  void ChangeRemotenessToProcess(
      mozilla::dom::ContentParent* aContentParent,
      const mozilla::dom::NavigationIsolationOptions& aOptions,
      mozilla::dom::BrowsingContextGroup* aGroup, mozilla::ErrorResult& rv);

  void SubframeCrashed();

  void RestoreFrameLoaderFromBFCache(nsFrameLoader* aNewFrameLoader);

  void UpdateFocusAndMouseEnterStateAfterFrameLoaderChange();

  void AttachFrameLoader(nsFrameLoader* aFrameLoader);
  void DetachFrameLoader(nsFrameLoader* aFrameLoader);
  // If aDestroyBFCached is true and aFrameLoader is the current frameloader
  // (mFrameLoader) then this will also call nsFrameLoader::Destroy on all the
  // other frame loaders in mFrameLoaderList and remove them from the list.
  void FrameLoaderDestroying(nsFrameLoader* aFrameLoader,
                             bool aDestroyBFCached);

 private:
  bool UseRemoteSubframes();

  // The enum class for determine how to handle previous BrowsingContext during
  // the change remoteness. It could be followings
  // 1. DONT_PRESERVE
  //    Create a whole new BrowsingContext.
  // 2. PRESERVE
  //    Preserve the previous BrowsingContext.
  enum class ChangeRemotenessContextType {
    DONT_PRESERVE = 0,
    PRESERVE = 1,
  };
  ChangeRemotenessContextType ShouldPreserveBrowsingContext(
      bool aIsRemote, bool aReplaceBrowsingContext);

  void ChangeRemotenessCommon(
      const ChangeRemotenessContextType& aContextType,
      const mozilla::dom::NavigationIsolationOptions& aOptions,
      bool aSwitchingInProgressLoad, bool aIsRemote,
      mozilla::dom::BrowsingContextGroup* aGroup,
      std::function<void()>& aFrameLoaderInit, mozilla::ErrorResult& aRv);

  void ChangeFrameLoaderCommon(mozilla::dom::Element* aOwner,
                               bool aRetainPaint);

  void UpdateFocusAndMouseEnterStateAfterFrameLoaderChange(
      mozilla::dom::Element* aOwner);

 protected:
  virtual ~nsFrameLoaderOwner() = default;
  RefPtr<nsFrameLoader> mFrameLoader;

  // The list contains all the nsFrameLoaders created for this owner or moved
  // from another nsFrameLoaderOwner which haven't been destroyed yet.
  // In particular it contains all the nsFrameLoaders which are in bfcache.
  mozilla::LinkedList<nsFrameLoader> mFrameLoaderList;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsFrameLoaderOwner, NS_FRAMELOADEROWNER_IID)

#endif  // nsFrameLoaderOwner_h_
