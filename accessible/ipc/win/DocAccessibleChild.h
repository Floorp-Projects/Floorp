/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_DocAccessibleChild_h
#define mozilla_a11y_DocAccessibleChild_h

#include "mozilla/a11y/COMPtrTypes.h"
#include "mozilla/a11y/DocAccessibleChildBase.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/mscom/Ptr.h"

namespace mozilla {
namespace a11y {

/*
 * These objects handle content side communication for an accessible document,
 * and their lifetime is the same as the document they represent.
 */
class DocAccessibleChild : public DocAccessibleChildBase {
 public:
  DocAccessibleChild(DocAccessible* aDoc, IProtocol* aManager);
  ~DocAccessibleChild();

  virtual void Shutdown() override;

  virtual ipc::IPCResult RecvParentCOMProxy(
      const IDispatchHolder& aParentCOMProxy) override;
  virtual ipc::IPCResult RecvEmulatedWindow(
      const WindowsHandle& aEmulatedWindowHandle,
      const IDispatchHolder& aEmulatedWindowCOMProxy) override;
  virtual ipc::IPCResult RecvRestoreFocus() override;

  HWND GetNativeWindowHandle() const;
  IDispatch* GetEmulatedWindowIAccessible() const {
    return mEmulatedWindowProxy.get();
  }

  IDispatch* GetParentIAccessible() const { return mParentProxy.get(); }

  bool SendEvent(const uint64_t& aID, const uint32_t& type);
  bool SendHideEvent(const uint64_t& aRootID, const bool& aFromUser);
  bool SendStateChangeEvent(const uint64_t& aID, const uint64_t& aState,
                            const bool& aEnabled);
  bool SendCaretMoveEvent(const uint64_t& aID, const int32_t& aOffset);
  bool SendCaretMoveEvent(const uint64_t& aID,
                          const LayoutDeviceIntRect& aCaretRect,
                          const int32_t& aOffset);
  bool SendFocusEvent(const uint64_t& aID);
  bool SendFocusEvent(const uint64_t& aID,
                      const LayoutDeviceIntRect& aCaretRect);
  bool SendTextChangeEvent(const uint64_t& aID, const nsString& aStr,
                           const int32_t& aStart, const uint32_t& aLen,
                           const bool& aIsInsert, const bool& aFromUser,
                           const bool aDoSync = false);
  bool SendSelectionEvent(const uint64_t& aID, const uint64_t& aWidgetID,
                          const uint32_t& aType);
  bool SendRoleChangedEvent(const a11y::role& aRole);
  bool SendScrollingEvent(const uint64_t& aID, const uint64_t& aType,
                          const uint32_t& aScrollX, const uint32_t& aScrollY,
                          const uint32_t& aMaxScrollX,
                          const uint32_t& aMaxScrollY);

  bool ConstructChildDocInParentProcess(DocAccessibleChild* aNewChildDoc,
                                        uint64_t aUniqueID, uint32_t aMsaaID);

  bool SendBindChildDoc(DocAccessibleChild* aChildDoc,
                        const uint64_t& aNewParentID);

 protected:
  virtual void MaybeSendShowEvent(ShowEventData& aData,
                                  bool aFromUser) override;

 private:
  void RemoveDeferredConstructor();

  bool IsConstructedInParentProcess() const { return mIsRemoteConstructed; }
  void SetConstructedInParentProcess() { mIsRemoteConstructed = true; }

  LayoutDeviceIntRect GetCaretRectFor(const uint64_t& aID);

  /**
   * DocAccessibleChild should not fire events until it has asynchronously
   * received the COM proxy for its parent. OTOH, content a11y may still be
   * attempting to fire events during this window of time. If this object does
   * not yet have its parent proxy, instead of immediately sending the events to
   * our parent, we enqueue them to mDeferredEvents. As soon as
   * RecvParentCOMProxy is called, we play back mDeferredEvents.
   */
  struct DeferredEvent {
    void Dispatch() { Dispatch(mTarget); }

    virtual ~DeferredEvent() {}

   protected:
    explicit DeferredEvent(DocAccessibleChild* aTarget) : mTarget(aTarget) {}

    virtual void Dispatch(DocAccessibleChild* aIPCDoc) = 0;

   private:
    DocAccessibleChild* mTarget;
  };

  void PushDeferredEvent(UniquePtr<DeferredEvent> aEvent);

  struct SerializedShow final : public DeferredEvent {
    SerializedShow(DocAccessibleChild* aTarget, ShowEventData& aEventData,
                   bool aFromUser)
        : DeferredEvent(aTarget),
          mEventData(aEventData.ID(), aEventData.Idx(),
                     nsTArray<AccessibleData>(), aEventData.EventSuppressed()),
          mFromUser(aFromUser) {
      // Since IPDL doesn't generate a move constructor for ShowEventData,
      // we move NewTree manually (ugh). We still construct with an empty
      // NewTree above so that the compiler catches any changes made to the
      // ShowEventData structure in IPDL.
      mEventData.NewTree() = std::move(aEventData.NewTree());
    }

    void Dispatch(DocAccessibleChild* aIPCDoc) override {
      Unused << aIPCDoc->SendShowEvent(mEventData, mFromUser);
    }

    ShowEventData mEventData;
    bool mFromUser;
  };

  struct SerializedHide final : public DeferredEvent {
    SerializedHide(DocAccessibleChild* aTarget, uint64_t aRootID,
                   bool aFromUser)
        : DeferredEvent(aTarget), mRootID(aRootID), mFromUser(aFromUser) {}

    void Dispatch(DocAccessibleChild* aIPCDoc) override {
      Unused << aIPCDoc->SendHideEvent(mRootID, mFromUser);
    }

    uint64_t mRootID;
    bool mFromUser;
  };

  struct SerializedStateChange final : public DeferredEvent {
    SerializedStateChange(DocAccessibleChild* aTarget, uint64_t aID,
                          uint64_t aState, bool aEnabled)
        : DeferredEvent(aTarget),
          mID(aID),
          mState(aState),
          mEnabled(aEnabled) {}

    void Dispatch(DocAccessibleChild* aIPCDoc) override {
      Unused << aIPCDoc->SendStateChangeEvent(mID, mState, mEnabled);
    }

    uint64_t mID;
    uint64_t mState;
    bool mEnabled;
  };

  struct SerializedCaretMove final : public DeferredEvent {
    SerializedCaretMove(DocAccessibleChild* aTarget, uint64_t aID,
                        const LayoutDeviceIntRect& aCaretRect, int32_t aOffset)
        : DeferredEvent(aTarget),
          mID(aID),
          mCaretRect(aCaretRect),
          mOffset(aOffset) {}

    void Dispatch(DocAccessibleChild* aIPCDoc) override {
      Unused << aIPCDoc->SendCaretMoveEvent(mID, mCaretRect, mOffset);
    }

    uint64_t mID;
    LayoutDeviceIntRect mCaretRect;
    int32_t mOffset;
  };

  struct SerializedFocus final : public DeferredEvent {
    SerializedFocus(DocAccessibleChild* aTarget, uint64_t aID,
                    const LayoutDeviceIntRect& aCaretRect)
        : DeferredEvent(aTarget), mID(aID), mCaretRect(aCaretRect) {}

    void Dispatch(DocAccessibleChild* aIPCDoc) override {
      Unused << aIPCDoc->SendFocusEvent(mID, mCaretRect);
    }

    uint64_t mID;
    LayoutDeviceIntRect mCaretRect;
  };

  struct SerializedTextChange final : public DeferredEvent {
    SerializedTextChange(DocAccessibleChild* aTarget, uint64_t aID,
                         const nsString& aStr, int32_t aStart, uint32_t aLen,
                         bool aIsInsert, bool aFromUser)
        : DeferredEvent(aTarget),
          mID(aID),
          mStr(aStr),
          mStart(aStart),
          mLen(aLen),
          mIsInsert(aIsInsert),
          mFromUser(aFromUser) {}

    void Dispatch(DocAccessibleChild* aIPCDoc) override {
      Unused << aIPCDoc->SendTextChangeEvent(mID, mStr, mStart, mLen, mIsInsert,
                                             mFromUser, false);
    }

    uint64_t mID;
    nsString mStr;
    int32_t mStart;
    uint32_t mLen;
    bool mIsInsert;
    bool mFromUser;
  };

  struct SerializedSelection final : public DeferredEvent {
    SerializedSelection(DocAccessibleChild* aTarget, uint64_t aID,
                        uint64_t aWidgetID, uint32_t aType)
        : DeferredEvent(aTarget),
          mID(aID),
          mWidgetID(aWidgetID),
          mType(aType) {}

    void Dispatch(DocAccessibleChild* aIPCDoc) override {
      Unused << aIPCDoc->SendSelectionEvent(mID, mWidgetID, mType);
    }

    uint64_t mID;
    uint64_t mWidgetID;
    uint32_t mType;
  };

  struct SerializedRoleChanged final : public DeferredEvent {
    explicit SerializedRoleChanged(DocAccessibleChild* aTarget,
                                   a11y::role aRole)
        : DeferredEvent(aTarget), mRole(aRole) {}

    void Dispatch(DocAccessibleChild* aIPCDoc) override {
      Unused << aIPCDoc->SendRoleChangedEvent(mRole);
    }

    a11y::role mRole;
  };

  struct SerializedScrolling final : public DeferredEvent {
    explicit SerializedScrolling(DocAccessibleChild* aTarget, uint64_t aID,
                                 uint64_t aType, uint32_t aScrollX,
                                 uint32_t aScrollY, uint32_t aMaxScrollX,
                                 uint32_t aMaxScrollY)
        : DeferredEvent(aTarget),
          mID(aID),
          mType(aType),
          mScrollX(aScrollX),
          mScrollY(aScrollY),
          mMaxScrollX(aMaxScrollX),
          mMaxScrollY(aMaxScrollY) {}

    void Dispatch(DocAccessibleChild* aIPCDoc) override {
      Unused << aIPCDoc->SendScrollingEvent(mID, mType, mScrollX, mScrollY,
                                            mMaxScrollX, mMaxScrollY);
    }

    uint64_t mID;
    uint64_t mType;
    uint32_t mScrollX;
    uint32_t mScrollY;
    uint32_t mMaxScrollX;
    uint32_t mMaxScrollY;
  };

  struct SerializedEvent final : public DeferredEvent {
    SerializedEvent(DocAccessibleChild* aTarget, uint64_t aID, uint32_t aType)
        : DeferredEvent(aTarget), mID(aID), mType(aType) {}

    void Dispatch(DocAccessibleChild* aIPCDoc) override {
      Unused << aIPCDoc->SendEvent(mID, mType);
    }

    uint64_t mID;
    uint32_t mType;
  };

  struct SerializedChildDocConstructor final : public DeferredEvent {
    SerializedChildDocConstructor(DocAccessibleChild* aIPCDoc,
                                  DocAccessibleChild* aParentIPCDoc,
                                  uint64_t aUniqueID, uint32_t aMsaaID)
        : DeferredEvent(aParentIPCDoc),
          mIPCDoc(aIPCDoc),
          mUniqueID(aUniqueID),
          mMsaaID(aMsaaID) {}

    void Dispatch(DocAccessibleChild* aParentIPCDoc) override {
      auto browserChild =
          static_cast<dom::BrowserChild*>(aParentIPCDoc->Manager());
      MOZ_ASSERT(browserChild);
      Unused << browserChild->SendPDocAccessibleConstructor(
          mIPCDoc, aParentIPCDoc, mUniqueID, mMsaaID, IAccessibleHolder());
      mIPCDoc->SetConstructedInParentProcess();
    }

    DocAccessibleChild* mIPCDoc;
    uint64_t mUniqueID;
    uint32_t mMsaaID;
  };

  friend struct SerializedChildDocConstructor;

  struct SerializedBindChildDoc final : public DeferredEvent {
    SerializedBindChildDoc(DocAccessibleChild* aParentDoc,
                           DocAccessibleChild* aChildDoc, uint64_t aNewParentID)
        : DeferredEvent(aParentDoc),
          mChildDoc(aChildDoc),
          mNewParentID(aNewParentID) {}

    void Dispatch(DocAccessibleChild* aParentIPCDoc) override {
      Unused << aParentIPCDoc->SendBindChildDoc(mChildDoc, mNewParentID);
    }

    DocAccessibleChild* mChildDoc;
    uint64_t mNewParentID;
  };

  struct SerializedShutdown final : public DeferredEvent {
    explicit SerializedShutdown(DocAccessibleChild* aTarget)
        : DeferredEvent(aTarget) {}

    void Dispatch(DocAccessibleChild* aIPCDoc) override { aIPCDoc->Shutdown(); }
  };

  bool mIsRemoteConstructed;
  mscom::ProxyUniquePtr<IDispatch> mParentProxy;
  mscom::ProxyUniquePtr<IDispatch> mEmulatedWindowProxy;
  nsTArray<UniquePtr<DeferredEvent>> mDeferredEvents;
  HWND mEmulatedWindowHandle;
};

}  // namespace a11y
}  // namespace mozilla

#endif  // mozilla_a11y_DocAccessibleChild_h
