/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include <functional>

#include "MobileViewportManager.h"
#include "mozilla/MVMContext.h"
#include "mozilla/dom/Event.h"

using namespace mozilla;

class MockMVMContext : public MVMContext {
  using AutoSizeFlag = nsViewportInfo::AutoSizeFlag;
  using AutoScaleFlag = nsViewportInfo::AutoScaleFlag;
  using ZoomFlag = nsViewportInfo::ZoomFlag;

  // A "layout function" is a function that computes the content size
  // as a function of the ICB size.
  using LayoutFunction = std::function<CSSSize(CSSSize aICBSize)>;

 public:
  // MVMContext methods we don't care to implement.
  MOCK_METHOD3(AddEventListener,
               void(const nsAString& aType, nsIDOMEventListener* aListener,
                    bool aUseCapture));
  MOCK_METHOD3(RemoveEventListener,
               void(const nsAString& aType, nsIDOMEventListener* aListener,
                    bool aUseCapture));
  MOCK_METHOD3(AddObserver, void(nsIObserver* aObserver, const char* aTopic,
                                 bool aOwnsWeak));
  MOCK_METHOD2(RemoveObserver,
               void(nsIObserver* aObserver, const char* aTopic));
  MOCK_METHOD0(Destroy, void());

  MOCK_METHOD1(SetVisualViewportSize, void(const CSSSize& aSize));
  MOCK_METHOD0(UpdateDisplayPortMargins, void());

  void SetMVM(MobileViewportManager* aMVM) { mMVM = aMVM; }

  // MVMContext method implementations.
  nsViewportInfo GetViewportInfo(const ScreenIntSize& aDisplaySize) const {
    // This is a very basic approximation of what Document::GetViewportInfo()
    // does in the most common cases.
    // Ideally, we would invoke the algorithm in Document::GetViewportInfo()
    // itself, but that would require refactoring it a bit to remove
    // dependencies on the actual Document which we don't have available in
    // this test harness.
    CSSSize viewportSize = mDisplaySize / mDeviceScale;
    if (mAutoSizeFlag == AutoSizeFlag::FixedSize) {
      viewportSize = CSSSize(mFixedViewportWidth,
                             mFixedViewportWidth * (float(mDisplaySize.height) /
                                                    mDisplaySize.width));
    }
    return nsViewportInfo(mDefaultScale, mMinScale, mMaxScale, viewportSize,
                          mAutoSizeFlag, mAutoScaleFlag, mZoomFlag);
  }
  CSSToLayoutDeviceScale CSSToDevPixelScale() const { return mDeviceScale; }
  float GetResolution() const { return mResolution; }
  bool SubjectMatchesDocument(nsISupports* aSubject) const { return true; }
  Maybe<CSSRect> CalculateScrollableRectForRSF() const {
    return Some(CSSRect(CSSPoint(), mContentSize));
  }
  bool IsResolutionUpdatedByApz() const { return false; }
  LayoutDeviceMargin ScrollbarAreaToExcludeFromCompositionBounds() const {
    return LayoutDeviceMargin();
  }
  Maybe<LayoutDeviceIntSize> GetContentViewerSize() const {
    return Some(mDisplaySize);
  }
  bool AllowZoomingForDocument() const { return true; }

  void SetResolutionAndScaleTo(float aResolution,
                               ResolutionChangeOrigin aOrigin) {
    mResolution = aResolution;
    mMVM->ResolutionUpdated(aOrigin);
  }
  void Reflow(const CSSSize& aNewSize, const CSSSize& aOldSize,
              ResizeEventFlag aResizeEventFlag) {
    mICBSize = aNewSize;
    mContentSize = mLayoutFunction(mICBSize);
  }

  // Allow test code to modify the input metrics.
  void SetMinScale(CSSToScreenScale aMinScale) { mMinScale = aMinScale; }
  void SetMaxScale(CSSToScreenScale aMaxScale) { mMaxScale = aMaxScale; }
  void SetInitialScale(CSSToScreenScale aInitialScale) {
    mDefaultScale = aInitialScale;
    mAutoScaleFlag = AutoScaleFlag::FixedScale;
  }
  void SetFixedViewportWidth(CSSCoord aWidth) {
    mFixedViewportWidth = aWidth;
    mAutoSizeFlag = AutoSizeFlag::FixedSize;
  }
  void SetDisplaySize(const LayoutDeviceIntSize& aNewDisplaySize) {
    mDisplaySize = aNewDisplaySize;
  }
  void SetLayoutFunction(const LayoutFunction& aLayoutFunction) {
    mLayoutFunction = aLayoutFunction;
  }

  // Allow test code to query the output metrics.
  CSSSize GetICBSize() const { return mICBSize; }
  CSSSize GetContentSize() const { return mContentSize; }

 private:
  // Input metrics, with some sensible defaults.
  LayoutDeviceIntSize mDisplaySize{300, 600};
  CSSToScreenScale mDefaultScale{1.0f};
  CSSToScreenScale mMinScale{0.25f};
  CSSToScreenScale mMaxScale{10.0f};
  CSSToLayoutDeviceScale mDeviceScale{1.0f};
  CSSCoord mFixedViewportWidth;
  AutoSizeFlag mAutoSizeFlag = AutoSizeFlag::AutoSize;
  AutoScaleFlag mAutoScaleFlag = AutoScaleFlag::AutoScale;
  ZoomFlag mZoomFlag = ZoomFlag::AllowZoom;
  // As a default layout function, just set the content size to the ICB size.
  LayoutFunction mLayoutFunction = [](CSSSize aICBSize) { return aICBSize; };

  // Output metrics.
  float mResolution = 1.0f;
  CSSSize mICBSize;
  CSSSize mContentSize;

  MobileViewportManager* mMVM = nullptr;
};

class MVMTester : public ::testing::Test {
 public:
  MVMTester()
      : mMVMContext(new MockMVMContext()),
        mMVM(new MobileViewportManager(mMVMContext)) {
    mMVMContext->SetMVM(mMVM.get());
  }

  void Resize(const LayoutDeviceIntSize& aNewDisplaySize) {
    mMVMContext->SetDisplaySize(aNewDisplaySize);
    mMVM->RequestReflow(false);
  }

 protected:
  RefPtr<MockMVMContext> mMVMContext;
  RefPtr<MobileViewportManager> mMVM;
};

TEST_F(MVMTester, ZoomBoundsRespectedAfterRotation_Bug1536755) {
  // Set up initial conditions.
  mMVMContext->SetDisplaySize(LayoutDeviceIntSize(600, 300));
  mMVMContext->SetInitialScale(CSSToScreenScale(1.0f));
  mMVMContext->SetMinScale(CSSToScreenScale(1.0f));
  mMVMContext->SetMaxScale(CSSToScreenScale(1.0f));
  // Set a layout function that simulates a page which is twice
  // as tall as it is wide.
  mMVMContext->SetLayoutFunction([](CSSSize aICBSize) {
    return CSSSize(aICBSize.width, aICBSize.width * 2);
  });

  // Perform an initial viewport computation and reflow, and
  // sanity-check the results.
  mMVM->SetInitialViewport();
  EXPECT_EQ(CSSSize(600, 300), mMVMContext->GetICBSize());
  EXPECT_EQ(CSSSize(600, 1200), mMVMContext->GetContentSize());
  EXPECT_EQ(1.0f, mMVMContext->GetResolution());

  // Now rotate the screen, and check that the minimum and maximum
  // scales are still respected after the rotation.
  Resize(LayoutDeviceIntSize(300, 600));
  EXPECT_EQ(CSSSize(300, 600), mMVMContext->GetICBSize());
  EXPECT_EQ(CSSSize(300, 600), mMVMContext->GetContentSize());
  EXPECT_EQ(1.0f, mMVMContext->GetResolution());
}

TEST_F(MVMTester, LandscapeToPortraitRotation_Bug1523844) {
  // Set up initial conditions.
  mMVMContext->SetDisplaySize(LayoutDeviceIntSize(300, 600));
  // Set a layout function that simulates a page with a fixed
  // content size that's as wide as the screen in one orientation
  // (and wider in the other).
  mMVMContext->SetLayoutFunction(
      [](CSSSize aICBSize) { return CSSSize(600, 1200); });

  // Simulate a "DOMMetaAdded" event being fired before calling
  // SetInitialViewport(). This matches what typically happens
  // during real usage (the MVM receives the "DOMMetaAdded"
  // before the "load", and it's the "load" that calls
  // SetInitialViewport()), and is important to trigger this
  // bug, because it causes the MVM to be stuck with an
  // "mRestoreResolution" (prior to the fix).
  mMVM->HandleDOMMetaAdded();

  // Perform an initial viewport computation and reflow, and
  // sanity-check the results.
  mMVM->SetInitialViewport();
  EXPECT_EQ(CSSSize(300, 600), mMVMContext->GetICBSize());
  EXPECT_EQ(CSSSize(600, 1200), mMVMContext->GetContentSize());
  EXPECT_EQ(0.5f, mMVMContext->GetResolution());

  // Rotate to landscape.
  Resize(LayoutDeviceIntSize(600, 300));
  EXPECT_EQ(CSSSize(600, 300), mMVMContext->GetICBSize());
  EXPECT_EQ(CSSSize(600, 1200), mMVMContext->GetContentSize());
  EXPECT_EQ(1.0f, mMVMContext->GetResolution());

  // Rotate back to portrait and check that we have returned
  // to the portrait resolution.
  Resize(LayoutDeviceIntSize(300, 600));
  EXPECT_EQ(CSSSize(300, 600), mMVMContext->GetICBSize());
  EXPECT_EQ(CSSSize(600, 1200), mMVMContext->GetContentSize());
  EXPECT_EQ(0.5f, mMVMContext->GetResolution());
}
