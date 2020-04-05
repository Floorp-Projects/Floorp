/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GTEST_MOCKWIDGET_H
#define GTEST_MOCKWIDGET_H

#include "mozilla/gfx/Point.h"
#include "mozilla/widget/InProcessCompositorWidget.h"
#include "nsBaseWidget.h"
#include "GLContext.h"
#include "GLContextProvider.h"

using mozilla::gl::CreateContextFlags;
using mozilla::gl::GLContext;
using mozilla::gl::GLContextProvider;

using mozilla::gfx::IntSize;

class MockWidget : public nsBaseWidget {
 public:
  MockWidget() : mCompWidth(0), mCompHeight(0) {}
  MockWidget(int aWidth, int aHeight)
      : mCompWidth(aWidth), mCompHeight(aHeight) {}
  NS_DECL_ISUPPORTS_INHERITED

  virtual LayoutDeviceIntRect GetClientBounds() override {
    return LayoutDeviceIntRect(0, 0, mCompWidth, mCompHeight);
  }
  virtual LayoutDeviceIntRect GetBounds() override { return GetClientBounds(); }

  void* GetNativeData(uint32_t aDataType) override {
    if (aDataType == NS_NATIVE_OPENGL_CONTEXT) {
      mozilla::gl::SurfaceCaps caps = mozilla::gl::SurfaceCaps::ForRGB();
      caps.preserve = false;
      caps.bpp16 = false;
      nsCString discardFailureId;
      RefPtr<GLContext> context = GLContextProvider::CreateOffscreen(
          IntSize(mCompWidth, mCompHeight), caps,
          CreateContextFlags::REQUIRE_COMPAT_PROFILE, &discardFailureId);
      return context.forget().take();
    }
    return nullptr;
  }

  virtual nsresult Create(nsIWidget* aParent, nsNativeWidget aNativeParent,
                          const LayoutDeviceIntRect& aRect,
                          nsWidgetInitData* aInitData = nullptr) override {
    return NS_OK;
  }
  virtual nsresult Create(nsIWidget* aParent, nsNativeWidget aNativeParent,
                          const DesktopIntRect& aRect,
                          nsWidgetInitData* aInitData = nullptr) override {
    return NS_OK;
  }
  virtual void Show(bool aState) override {}
  virtual bool IsVisible() const override { return true; }
  virtual void Move(double aX, double aY) override {}
  virtual void Resize(double aWidth, double aHeight, bool aRepaint) override {}
  virtual void Resize(double aX, double aY, double aWidth, double aHeight,
                      bool aRepaint) override {}

  virtual void Enable(bool aState) override {}
  virtual bool IsEnabled() const override { return true; }
  virtual void SetFocus(Raise, mozilla::dom::CallerType aCallerType) override {}
  virtual nsresult ConfigureChildren(
      const nsTArray<Configuration>& aConfigurations) override {
    return NS_OK;
  }
  virtual void Invalidate(const LayoutDeviceIntRect& aRect) override {}
  virtual nsresult SetTitle(const nsAString& title) override { return NS_OK; }
  virtual LayoutDeviceIntPoint WidgetToScreenOffset() override {
    return LayoutDeviceIntPoint(0, 0);
  }
  virtual nsresult DispatchEvent(mozilla::WidgetGUIEvent* aEvent,
                                 nsEventStatus& aStatus) override {
    return NS_OK;
  }
  virtual void SetInputContext(const InputContext& aContext,
                               const InputContextAction& aAction) override {}
  virtual InputContext GetInputContext() override { abort(); }

 private:
  ~MockWidget() = default;

  int mCompWidth;
  int mCompHeight;
};

#endif
