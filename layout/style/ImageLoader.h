/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// A class that handles style system image loads (other image loads are handled
// by the nodes in the content tree).

#ifndef mozilla_css_ImageLoader_h___
#define mozilla_css_ImageLoader_h___

#include "mozilla/CORSMode.h"
#include "nsClassHashtable.h"
#include "nsHashKeys.h"
#include "nsIFrame.h"
#include "nsIReflowCallback.h"
#include "nsTArray.h"
#include "imgIRequest.h"
#include "imgINotificationObserver.h"
#include "mozilla/Attributes.h"

class imgIContainer;
class nsPresContext;
class nsIURI;
class nsIPrincipal;

namespace mozilla {
struct MediaFeatureChange;
namespace dom {
class Document;
}

namespace css {

/**
 * NOTE: All methods must be called from the main thread unless otherwise
 * specified.
 */
class ImageLoader final {
 public:
  static void Init();
  static void Shutdown();

  // We also associate flags alongside frames in the request-to-frames hashmap.
  // These are used for special handling of events for requests.
  typedef uint32_t FrameFlags;
  enum {
    REQUEST_REQUIRES_REFLOW = 1u << 0,
    REQUEST_HAS_BLOCKED_ONLOAD = 1u << 1,
  };

  explicit ImageLoader(dom::Document* aDocument) : mDocument(aDocument) {
    MOZ_ASSERT(mDocument);
  }

  NS_INLINE_DECL_REFCOUNTING(ImageLoader)

  void DropDocumentReference();

  void AssociateRequestToFrame(imgIRequest* aRequest, nsIFrame* aFrame,
                               FrameFlags aFlags);

  void DisassociateRequestFromFrame(imgIRequest* aRequest, nsIFrame* aFrame);

  void DropRequestsForFrame(nsIFrame* aFrame);

  void SetAnimationMode(uint16_t aMode);

  // The prescontext for this ImageLoader's document. We need it to be passed
  // in because this can be called during presentation destruction after the
  // presshell pointer on the document has been cleared.
  void ClearFrames(nsPresContext* aPresContext);

  // Triggers an image load.
  static already_AddRefed<imgRequestProxy> LoadImage(
      const StyleComputedImageUrl&, dom::Document&);

  // Usually, only one style value owns a given proxy. However, we have a hack
  // to share image proxies in chrome documents under some circumstances. We
  // need to keep track of this so that we don't stop tracking images too early.
  //
  // In practice it shouldn't matter as these chrome images are mostly static,
  // but it is always good to keep sanity.
  static void NoteSharedLoad(imgRequestProxy*);

  // Undoes what `LoadImage` does.
  static void UnloadImage(imgRequestProxy*);

  // This is called whenever an image we care about notifies the
  // GlobalImageObserver.
  void Notify(imgIRequest*, int32_t aType, const nsIntRect* aData);

 private:
  // Called when we stop caring about a given request.
  void DeregisterImageRequest(imgIRequest*, nsPresContext*);

  // This callback is used to unblock document onload after a reflow
  // triggered from an image load.
  struct ImageReflowCallback final : public nsIReflowCallback {
    RefPtr<ImageLoader> mLoader;
    WeakFrame mFrame;
    nsCOMPtr<imgIRequest> const mRequest;

    ImageReflowCallback(ImageLoader* aLoader, nsIFrame* aFrame,
                        imgIRequest* aRequest)
        : mLoader(aLoader), mFrame(aFrame), mRequest(aRequest) {}

    bool ReflowFinished() override;
    void ReflowCallbackCanceled() override;
  };

  ~ImageLoader() = default;

  // We need to be able to look up the frames associated with a request (for
  // delivering notifications) and the requests associated with a frame (when
  // the frame goes away). Thus we maintain hashtables going both ways.  These
  // should always be in sync.

  struct FrameWithFlags {
    explicit FrameWithFlags(nsIFrame* aFrame) : mFrame(aFrame), mFlags(0) {
      MOZ_ASSERT(mFrame);
    }
    nsIFrame* const mFrame;
    FrameFlags mFlags;
  };

  // A helper class to compare FrameWithFlags by comparing mFrame and
  // ignoring mFlags.
  class FrameOnlyComparator {
   public:
    bool Equals(const FrameWithFlags& aElem1,
                const FrameWithFlags& aElem2) const {
      return aElem1.mFrame == aElem2.mFrame;
    }

    bool LessThan(const FrameWithFlags& aElem1,
                  const FrameWithFlags& aElem2) const {
      return aElem1.mFrame < aElem2.mFrame;
    }
  };

  typedef nsTArray<FrameWithFlags> FrameSet;
  typedef nsTArray<nsCOMPtr<imgIRequest>> RequestSet;
  typedef nsClassHashtable<nsISupportsHashKey, FrameSet> RequestToFrameMap;
  typedef nsClassHashtable<nsPtrHashKey<nsIFrame>, RequestSet>
      FrameToRequestMap;

  nsPresContext* GetPresContext();

  void RequestPaintIfNeeded(FrameSet* aFrameSet, imgIRequest* aRequest,
                            bool aForcePaint);
  void UnblockOnloadIfNeeded(nsIFrame* aFrame, imgIRequest* aRequest);
  void RequestReflowIfNeeded(FrameSet* aFrameSet, imgIRequest* aRequest);
  void RequestReflowOnFrame(FrameWithFlags* aFwf, imgIRequest* aRequest);

  void OnSizeAvailable(imgIRequest* aRequest, imgIContainer* aImage);
  void OnFrameComplete(imgIRequest* aRequest);
  void OnImageIsAnimated(imgIRequest* aRequest);
  void OnFrameUpdate(imgIRequest* aRequest);
  void OnLoadComplete(imgIRequest* aRequest);

  // Helpers for DropRequestsForFrame / DisassociateRequestFromFrame above.
  void RemoveRequestToFrameMapping(imgIRequest* aRequest, nsIFrame* aFrame);
  void RemoveFrameToRequestMapping(imgIRequest* aRequest, nsIFrame* aFrame);

  // A map of imgIRequests to the nsIFrames that are using them.
  RequestToFrameMap mRequestToFrameMap;

  // A map of nsIFrames to the imgIRequests they use.
  FrameToRequestMap mFrameToRequestMap;

  // A weak pointer to our document. Nulled out by DropDocumentReference.
  dom::Document* mDocument;
};

}  // namespace css
}  // namespace mozilla

#endif /* mozilla_css_ImageLoader_h___ */
