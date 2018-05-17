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
class nsIFrame;
class nsIDocument;
class nsPresContext;
class nsIURI;
class nsIPrincipal;

namespace mozilla {
namespace css {

struct ImageValue;

class ImageLoader final : public imgINotificationObserver
{
public:
  // We also associate flags alongside frames in the request-to-frames hashmap.
  // These are used for special handling of events for requests.
  typedef uint32_t FrameFlags;
  enum {
    REQUEST_REQUIRES_REFLOW      = 1u << 0,
    REQUEST_HAS_BLOCKED_ONLOAD   = 1u << 1,
  };

  typedef mozilla::css::ImageValue Image;

  explicit ImageLoader(nsIDocument* aDocument)
  : mDocument(aDocument),
    mInClone(false)
  {
    MOZ_ASSERT(mDocument);
  }

  NS_DECL_ISUPPORTS
  NS_DECL_IMGINOTIFICATIONOBSERVER

  void DropDocumentReference();

  void MaybeRegisterCSSImage(Image* aImage);
  void DeregisterCSSImage(Image* aImage);

  void AssociateRequestToFrame(imgIRequest* aRequest,
                               nsIFrame* aFrame,
                               FrameFlags aFlags);

  void DisassociateRequestFromFrame(imgIRequest* aRequest,
                                    nsIFrame* aFrame);

  void DropRequestsForFrame(nsIFrame* aFrame);

  void SetAnimationMode(uint16_t aMode);

  // The prescontext for this ImageLoader's document. We need it to be passed
  // in because this can be called during presentation destruction after the
  // presshell pointer on the document has been cleared.
  void ClearFrames(nsPresContext* aPresContext);

  void LoadImage(nsIURI* aURI, nsIPrincipal* aPrincipal, nsIURI* aReferrer,
                 Image* aCSSValue, CORSMode aCorsMode);

  void DestroyRequest(imgIRequest* aRequest);

  void FlushUseCounters();

private:
  // This callback is used to unblock document onload after a reflow
  // triggered from an image load.
  struct ImageReflowCallback final : public nsIReflowCallback
  {
    RefPtr<ImageLoader> mLoader;
    WeakFrame mFrame;
    nsCOMPtr<imgIRequest> const mRequest;

    ImageReflowCallback(ImageLoader* aLoader,
                        nsIFrame* aFrame,
                        imgIRequest* aRequest)
    : mLoader(aLoader)
    , mFrame(aFrame)
    , mRequest(aRequest)
    {}

    bool ReflowFinished() override;
    void ReflowCallbackCanceled() override;
  };

  ~ImageLoader() {}

  // We need to be able to look up the frames associated with a request (for
  // delivering notifications) and the requests associated with a frame (when
  // the frame goes away). Thus we maintain hashtables going both ways.  These
  // should always be in sync.

  struct FrameWithFlags {
    explicit FrameWithFlags(nsIFrame* aFrame)
    : mFrame(aFrame),
      mFlags(0)
    {
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
                  const FrameWithFlags& aElem2) const
      { return aElem1.mFrame == aElem2.mFrame; }

      bool LessThan(const FrameWithFlags& aElem1,
                    const FrameWithFlags& aElem2) const
      { return aElem1.mFrame < aElem2.mFrame; }
  };

  typedef nsTArray<FrameWithFlags> FrameSet;
  typedef nsTArray<nsCOMPtr<imgIRequest> > RequestSet;
  typedef nsTHashtable<nsPtrHashKey<Image> > ImageHashSet;
  typedef nsClassHashtable<nsISupportsHashKey,
                           FrameSet> RequestToFrameMap;
  typedef nsClassHashtable<nsPtrHashKey<nsIFrame>,
                           RequestSet> FrameToRequestMap;

  void AddImage(Image* aCSSImage);
  void RemoveImage(Image* aCSSImage);

  nsPresContext* GetPresContext();

  void DoRedraw(FrameSet* aFrameSet, bool aForcePaint);
  void UnblockOnloadIfNeeded(nsIFrame* aFrame, imgIRequest* aRequest);
  void RequestReflowIfNeeded(FrameSet* aFrameSet, imgIRequest* aRequest);
  void RequestReflowOnFrame(FrameWithFlags* aFwf, imgIRequest* aRequest);

  nsresult OnSizeAvailable(imgIRequest* aRequest, imgIContainer* aImage);
  nsresult OnFrameComplete(imgIRequest* aRequest);
  nsresult OnImageIsAnimated(imgIRequest* aRequest);
  nsresult OnFrameUpdate(imgIRequest* aRequest);
  nsresult OnLoadComplete(imgIRequest* aRequest);

  // Helpers for DropRequestsForFrame / DisassociateRequestFromFrame above.
  void RemoveRequestToFrameMapping(imgIRequest* aRequest, nsIFrame* aFrame);
  void RemoveFrameToRequestMapping(imgIRequest* aRequest, nsIFrame* aFrame);

  // A map of imgIRequests to the nsIFrames that are using them.
  RequestToFrameMap mRequestToFrameMap;

  // A map of nsIFrames to the imgIRequests they use.
  FrameToRequestMap mFrameToRequestMap;

  // A weak pointer to our document. Nulled out by DropDocumentReference.
  nsIDocument* mDocument;

  // The set of all nsCSSValue::Images (whether they're associated a frame or
  // not).  We'll need this when we go away to remove any requests associated
  // with our document from those Images.
  ImageHashSet mImages;

  // Are we cloning?  If so, ignore any notifications we get.
  bool mInClone;
};

} // namespace css
} // namespace mozilla

#endif /* mozilla_css_ImageLoader_h___ */
