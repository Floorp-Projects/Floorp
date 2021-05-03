/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_AccessibleWrap_h_
#define mozilla_a11y_AccessibleWrap_h_

#include "nsCOMPtr.h"
#include "LocalAccessible.h"
#include "MsaaAccessible.h"
#include "mozilla/a11y/AccessibleHandler.h"
#include "mozilla/a11y/RemoteAccessible.h"
#include "mozilla/Attributes.h"
#include "mozilla/mscom/Utils.h"
#include "mozilla/StaticPtr.h"
#include "nsXULAppAPI.h"
#include "Units.h"

#if defined(__GNUC__) || defined(__clang__)
// Inheriting from both XPCOM and MSCOM interfaces causes a lot of warnings
// about virtual functions being hidden by each other. This is done by
// design, so silence the warning.
#  pragma GCC diagnostic ignored "-Woverloaded-virtual"
#endif

namespace mozilla {
namespace a11y {
class DocRemoteAccessibleWrap;

class AccessibleWrap : public LocalAccessible {
 public:  // construction, destruction
  AccessibleWrap(nsIContent* aContent, DocAccessible* aDoc);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

 public:
  // LocalAccessible
  virtual nsresult HandleAccEvent(AccEvent* aEvent) override;
  virtual void Shutdown() override;

  // Helper methods
  /**
   * System caret support: update the Windows caret position.
   * The system caret works more universally than the MSAA caret
   * For example, Window-Eyes, JAWS, ZoomText and Windows Tablet Edition use it
   * We will use an invisible system caret.
   * Gecko is still responsible for drawing its own caret
   */
  void UpdateSystemCaretFor(LocalAccessible* aAccessible);
  static void UpdateSystemCaretFor(RemoteAccessible* aProxy,
                                   const LayoutDeviceIntRect& aCaretRect);

 private:
  static void UpdateSystemCaretFor(HWND aCaretWnd,
                                   const LayoutDeviceIntRect& aCaretRect);

 public:
  /**
   * Determine whether this is the root accessible for its HWND.
   */
  bool IsRootForHWND();

  MsaaAccessible* GetMsaa();
  virtual void GetNativeInterface(void** aOutAccessible) override;

  static void SetHandlerControl(DWORD aPid, RefPtr<IHandlerControl> aCtrl);

  static void InvalidateHandlers();

  bool DispatchTextChangeToHandler(bool aIsInsert, const nsString& aText,
                                   int32_t aStart, uint32_t aLen);

 protected:
  virtual ~AccessibleWrap() = default;

  RefPtr<MsaaAccessible> mMsaa;

  struct HandlerControllerData final {
    HandlerControllerData(DWORD aPid, RefPtr<IHandlerControl>&& aCtrl)
        : mPid(aPid), mCtrl(std::move(aCtrl)) {
      mIsProxy = mozilla::mscom::IsProxy(mCtrl);
    }

    HandlerControllerData(HandlerControllerData&& aOther)
        : mPid(aOther.mPid),
          mIsProxy(aOther.mIsProxy),
          mCtrl(std::move(aOther.mCtrl)) {}

    bool operator==(const HandlerControllerData& aOther) const {
      return mPid == aOther.mPid;
    }

    bool operator==(const DWORD& aPid) const { return mPid == aPid; }

    DWORD mPid;
    bool mIsProxy;
    RefPtr<IHandlerControl> mCtrl;
  };

  static StaticAutoPtr<nsTArray<HandlerControllerData>> sHandlerControllers;
};

static inline AccessibleWrap* WrapperFor(const RemoteAccessible* aProxy) {
  return reinterpret_cast<AccessibleWrap*>(aProxy->GetWrapper());
}

}  // namespace a11y
}  // namespace mozilla

#endif
