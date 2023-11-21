/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_LargestContentfulPaint_h___
#define mozilla_dom_LargestContentfulPaint_h___

#include "nsCycleCollectionParticipant.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/PerformanceEntry.h"
#include "mozilla/dom/PerformanceLargestContentfulPaintBinding.h"

#include "imgRequestProxy.h"

class nsTextFrame;
namespace mozilla::dom {

static constexpr nsLiteralString kLargestContentfulPaintName =
    u"largest-contentful-paint"_ns;

class Performance;
class PerformanceMainThread;

struct LCPImageEntryKey {
  LCPImageEntryKey(Element* aElement, imgRequestProxy* aImgRequestProxy)
      : mElement(do_GetWeakReference(aElement)),
        mImageRequestProxy(aImgRequestProxy) {
    MOZ_ASSERT(aElement);
    MOZ_ASSERT(aImgRequestProxy);

    mHash = mozilla::HashGeneric(reinterpret_cast<uintptr_t>(aElement),
                                 reinterpret_cast<uintptr_t>(aImgRequestProxy));
  }

  LCPImageEntryKey(const LCPImageEntryKey& aLCPImageEntryKey) {
    mElement = aLCPImageEntryKey.mElement;
    mImageRequestProxy = aLCPImageEntryKey.mImageRequestProxy;
    mHash = aLCPImageEntryKey.mHash;
  }

  Element* GetElement() const {
    nsCOMPtr<Element> element = do_QueryReferent(mElement);
    return element;
  }

  imgRequestProxy* GetImgRequestProxy() const {
    return static_cast<imgRequestProxy*>(mImageRequestProxy.get());
  }

  bool operator==(const LCPImageEntryKey& aOther) const {
    imgRequestProxy* imgRequest = GetImgRequestProxy();
    if (!imgRequest) {
      return false;
    }

    imgRequestProxy* otherImgRequest = aOther.GetImgRequestProxy();
    if (!otherImgRequest) {
      return false;
    }

    Element* element = GetElement();
    if (!element) {
      return false;
    }

    Element* otherElement = aOther.GetElement();
    if (!otherElement) {
      return false;
    }

    return element == otherElement && imgRequest == otherImgRequest;
  }

  bool Equals(const Element* aElement,
              const imgRequestProxy* aImgRequestProxy) const {
    Element* element = GetElement();
    if (!element || !mImageRequestProxy) {
      return false;
    }

    return element == aElement && mImageRequestProxy == mImageRequestProxy;
  }

  nsWeakPtr mElement;

  WeakPtr<PreloaderBase> mImageRequestProxy;

  PLDHashNumber mHash = 0;

  ~LCPImageEntryKey() = default;
};

struct LCPTextFrameHelper final {
  static void MaybeUnionTextFrame(nsTextFrame* aTextFrame,
                                  const nsRect& aRelativeToSelfRect);
};

class ImagePendingRendering final {
 public:
  ImagePendingRendering(const LCPImageEntryKey& aLCPImageEntryKey,
                        DOMHighResTimeStamp aLoadTime);

  Element* GetElement() const { return mLCPImageEntryKey.GetElement(); }

  imgRequestProxy* GetImgRequestProxy() const {
    return mLCPImageEntryKey.GetImgRequestProxy();
  }

  LCPImageEntryKey mLCPImageEntryKey;
  DOMHighResTimeStamp mLoadTime;
};

class LCPEntryHashEntry : public PLDHashEntryHdr {
 public:
  typedef const LCPImageEntryKey& KeyType;
  typedef const LCPImageEntryKey* KeyTypePointer;

  explicit LCPEntryHashEntry(KeyTypePointer aKey) : mKey(*aKey) {}
  LCPEntryHashEntry(LCPEntryHashEntry&&) = default;

  ~LCPEntryHashEntry() = default;

  bool KeyEquals(KeyTypePointer aKey) const { return mKey == *aKey; }

  KeyType GetKey() const { return mKey; }

  static KeyTypePointer KeyToPointer(KeyType& aKey) { return &aKey; }

  static PLDHashNumber HashKey(KeyTypePointer aKey) { return aKey->mHash; }
  enum { ALLOW_MEMMOVE = true };

  LCPImageEntryKey mKey;
};

class LCPHelpers final {
 public:
  // Creates the LCP Entry for images with all information except the size of
  // the element. The size of the image is unknown at the moment. The entry is
  // not going to be queued in this function.
  static void CreateLCPEntryForImage(
      PerformanceMainThread* aPerformance, Element* aElement,
      imgRequestProxy* aRequestProxy, const DOMHighResTimeStamp aLoadTime,
      const DOMHighResTimeStamp aRenderTime,
      const LCPImageEntryKey& aContentIdentifier);

  // Called when the size of the image is known.
  static void FinalizeLCPEntryForImage(Element* aContainingBlock,
                                       imgRequestProxy* aImgRequestProxy,
                                       const nsRect& aTargetRectRelativeToSelf);

  static void FinalizeLCPEntryForText(PerformanceMainThread* aPerformance,
                                      const DOMHighResTimeStamp aRenderTime,
                                      Element* aContainingBlock,
                                      const nsRect& aTargetRectRelativeToSelf,
                                      const nsPresContext* aPresContext);

  static bool IsQualifiedImageRequest(imgRequest* aRequest,
                                      Element* aContainingElement);

 private:
  static bool CanFinalizeLCPEntry(const nsIFrame* aFrame);
};

// https://w3c.github.io/largest-contentful-paint/
class LargestContentfulPaint final : public PerformanceEntry {
 public:
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(LargestContentfulPaint,
                                           PerformanceEntry)

  LargestContentfulPaint(PerformanceMainThread* aPerformance,
                         const DOMHighResTimeStamp aRenderTime,
                         const DOMHighResTimeStamp aLoadTime,
                         const unsigned long aSize, nsIURI* aURI,
                         Element* aElement,
                         const Maybe<const LCPImageEntryKey>& aLCPImageEntryKey,
                         bool aShouldExposeRenderTime);

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  DOMHighResTimeStamp RenderTime() const;
  DOMHighResTimeStamp LoadTime() const;
  DOMHighResTimeStamp StartTime() const override;

  unsigned long Size() const { return mSize; }
  void GetId(nsAString& aId) const {
    if (mId) {
      mId->ToString(aId);
    }
  }
  void GetUrl(nsAString& aUrl);

  Element* GetElement() const;

  static Element* GetContainingBlockForTextFrame(const nsTextFrame* aTextFrame);

  void UpdateSize(const Element* aContainingBlock,
                  const nsRect& aTargetRectRelativeToSelf,
                  const PerformanceMainThread* aPerformance, bool aIsImage);

  void BufferEntryIfNeeded() override;

  static void MaybeProcessImageForElementTiming(imgRequestProxy* aRequest,
                                                Element* aElement);

  void QueueEntry();

  Maybe<LCPImageEntryKey>& GetLCPImageEntryKey() { return mLCPImageEntryKey; }

 private:
  ~LargestContentfulPaint() = default;

  void ReportLCPToNavigationTimings();

  RefPtr<PerformanceMainThread> mPerformance;

  // This is always set but only exposed to web content if
  // mShouldExposeRenderTime is true.
  DOMHighResTimeStamp mRenderTime;
  DOMHighResTimeStamp mLoadTime;
  // This is set to false when for security reasons web content it not allowed
  // to see the RenderTime.
  const bool mShouldExposeRenderTime;
  unsigned long mSize;
  nsCOMPtr<nsIURI> mURI;

  nsWeakPtr mElement;
  RefPtr<nsAtom> mId;

  Maybe<LCPImageEntryKey> mLCPImageEntryKey;
};
}  // namespace mozilla::dom
#endif
