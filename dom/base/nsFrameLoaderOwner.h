/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFrameLoaderOwner_h_
#define nsFrameLoaderOwner_h_

#include "nsISupports.h"

class nsFrameLoader;
namespace mozilla {
class ErrorResult;
namespace dom {
class BrowsingContext;
class BrowserBridgeChild;
struct RemotenessOptions;
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

  // Destroy (if it exists) and recreate our frameloader, based on new
  // remoteness requirements. This should follow the same path as
  // tabbrowser.js's updateBrowserRemoteness, including running the same logic
  // and firing the same events as unbinding a XULBrowserElement from the tree.
  // However, this method is available from backend and does not manipulate the
  // DOM.
  void ChangeRemoteness(const mozilla::dom::RemotenessOptions& aOptions,
                        mozilla::ErrorResult& rv);

  void ChangeRemotenessWithBridge(mozilla::dom::BrowserBridgeChild* aBridge,
                                  mozilla::ErrorResult& rv);

 private:
  bool UseRemoteSubframes();
  bool ShouldPreserveBrowsingContext(
      const mozilla::dom::RemotenessOptions& aOptions);

  // The enum class for determine how to handle previous BrowsingContext during
  // the change remoteness. It could be followings
  // 1. DONT_PRESERVE
  //    Create a whole new BrowsingContext.
  // 2. DONT_PRESERVE_BUT_PROPAGETE
  //    Create a whole new BrowsingContext, but propagate necessary feilds from
  //    previous BrowsingContext, i.e. COEP.
  // 3. PRESERVE
  //    Preserve the previous BrowsingContext.
  enum class ChangeRemotenessContextType {
    DONT_PRESERVE = 0,
    DONT_PRESERVE_BUT_PROPAGATE = 1,
    PRESERVE = 2,
  };
  void ChangeRemotenessCommon(const ChangeRemotenessContextType& aContextType,
                              bool aSwitchingInProgressLoad,
                              const nsAString& aRemoteType,
                              std::function<void()>& aFrameLoaderInit,
                              mozilla::ErrorResult& aRv);

 protected:
  virtual ~nsFrameLoaderOwner() = default;
  RefPtr<nsFrameLoader> mFrameLoader;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsFrameLoaderOwner, NS_FRAMELOADEROWNER_IID)

#endif  // nsFrameLoaderOwner_h_
