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

  // MVMContext method implementations.
  nsViewportInfo GetViewportInfo(const ScreenIntSize& aDisplaySize) const {
    return nsViewportInfo(mDefaultScale, mMinScale, mMaxScale,
                          mDisplaySize / mDeviceScale, mAutoSizeFlag,
                          mAutoScaleFlag, mZoomFlag);
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
  AutoSizeFlag mAutoSizeFlag = AutoSizeFlag::AutoSize;
  AutoScaleFlag mAutoScaleFlag = AutoScaleFlag::AutoScale;
  ZoomFlag mZoomFlag = ZoomFlag::AllowZoom;
  // As a default layout function, just set the content size to the ICB size.
  LayoutFunction mLayoutFunction = [](CSSSize aICBSize) { return aICBSize; };

  // Output metrics.
  float mResolution = 1.0f;
  CSSSize mICBSize;
  CSSSize mContentSize;
};

class MVMTester : public ::testing::Test {
 public:
  MVMTester()
      : mMVMContext(new MockMVMContext()),
        mMVM(new MobileViewportManager(mMVMContext)) {}

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
