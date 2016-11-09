/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_DocAccessibleChild_h
#define mozilla_a11y_DocAccessibleChild_h

#include "mozilla/a11y/COMPtrTypes.h"
#include "mozilla/a11y/DocAccessibleChildBase.h"
#include "mozilla/mscom/Ptr.h"

namespace mozilla {
namespace a11y {

/*
 * These objects handle content side communication for an accessible document,
 * and their lifetime is the same as the document they represent.
 */
class DocAccessibleChild : public DocAccessibleChildBase
{
public:
  explicit DocAccessibleChild(DocAccessible* aDoc);
  ~DocAccessibleChild();

  virtual bool RecvParentCOMProxy(const IAccessibleHolder& aParentCOMProxy) override;

  IAccessible* GetParentIAccessible() const { return mParentProxy.get(); }

  bool SendEvent(const uint64_t& aID, const uint32_t& type);
  bool SendHideEvent(const uint64_t& aRootID, const bool& aFromUser);
  bool SendStateChangeEvent(const uint64_t& aID, const uint64_t& aState,
                            const bool& aEnabled);
  bool SendCaretMoveEvent(const uint64_t& aID, const int32_t& aOffset);
  bool SendTextChangeEvent(const uint64_t& aID, const nsString& aStr,
                           const int32_t& aStart, const uint32_t& aLen,
                           const bool& aIsInsert, const bool& aFromUser);
  bool SendSelectionEvent(const uint64_t& aID, const uint64_t& aWidgetID,
                          const uint32_t& aType);

protected:
  virtual void MaybeSendShowEvent(ShowEventData& aData, bool aFromUser) override;

private:
  /**
   * DocAccessibleChild should not fire events until it has asynchronously
   * received the COM proxy for its parent. OTOH, content a11y may still be
   * attempting to fire events during this window of time. If this object does
   * not yet have its parent proxy, instead of immediately sending the events to
   * our parent, we enqueue them to mDeferredEvents. As soon as
   * RecvParentCOMProxy is called, we play back mDeferredEvents.
   */
  struct DeferredEvent
  {
    virtual void Dispatch(DocAccessibleChild* aIPCDoc) = 0;
    virtual ~DeferredEvent() {}
  };

  struct SerializedShow final : public DeferredEvent
  {
    SerializedShow(ShowEventData& aEventData, bool aFromUser)
      : mEventData(aEventData.ID(), aEventData.Idx(), nsTArray<AccessibleData>())
      , mFromUser(aFromUser)
    {
      // Since IPDL doesn't generate a move constructor for ShowEventData,
      // we move NewTree manually (ugh). We still construct with an empty
      // NewTree above so that the compiler catches any changes made to the
      // ShowEventData structure in IPDL.
      mEventData.NewTree() = Move(aEventData.NewTree());
    }

    void Dispatch(DocAccessibleChild* aIPCDoc) override
    {
      Unused << aIPCDoc->SendShowEvent(mEventData, mFromUser);
    }

    ShowEventData mEventData;
    bool          mFromUser;
  };

  struct SerializedHide final : public DeferredEvent
  {
    SerializedHide(uint64_t aRootID, bool aFromUser)
      : mRootID(aRootID)
      , mFromUser(aFromUser)
    {}

    void Dispatch(DocAccessibleChild* aIPCDoc) override
    {
      Unused << aIPCDoc->SendHideEvent(mRootID, mFromUser);
    }

    uint64_t  mRootID;
    bool      mFromUser;
  };

  struct SerializedStateChange final : public DeferredEvent
  {
    SerializedStateChange(uint64_t aID, uint64_t aState, bool aEnabled)
      : mID(aID)
      , mState(aState)
      , mEnabled(aEnabled)
    {}

    void Dispatch(DocAccessibleChild* aIPCDoc) override
    {
      Unused << aIPCDoc->SendStateChangeEvent(mID, mState, mEnabled);
    }

    uint64_t  mID;
    uint64_t  mState;
    bool      mEnabled;
  };

  struct SerializedCaretMove final : public DeferredEvent
  {
    SerializedCaretMove(uint64_t aID, int32_t aOffset)
      : mID(aID)
      , mOffset(aOffset)
    {}

    void Dispatch(DocAccessibleChild* aIPCDoc) override
    {
      Unused << aIPCDoc->SendCaretMoveEvent(mID, mOffset);
    }

    uint64_t  mID;
    int32_t   mOffset;
  };

  struct SerializedTextChange final : public DeferredEvent
  {
    SerializedTextChange(uint64_t aID, const nsString& aStr, int32_t aStart,
                         uint32_t aLen, bool aIsInsert, bool aFromUser)
      : mID(aID)
      , mStr(aStr)
      , mStart(aStart)
      , mLen(aLen)
      , mIsInsert(aIsInsert)
      , mFromUser(aFromUser)
    {}

    void Dispatch(DocAccessibleChild* aIPCDoc) override
    {
      Unused << aIPCDoc->SendTextChangeEvent(mID, mStr, mStart, mLen, mIsInsert,
                                             mFromUser);
    }

    uint64_t  mID;
    nsString  mStr;
    int32_t   mStart;
    uint32_t  mLen;
    bool      mIsInsert;
    bool      mFromUser;
  };

  struct SerializedSelection final : public DeferredEvent
  {
    SerializedSelection(uint64_t aID, uint64_t aWidgetID, uint32_t aType)
      : mID(aID)
      , mWidgetID(aWidgetID)
      , mType(aType)
    {}

    void Dispatch(DocAccessibleChild* aIPCDoc) override
    {
      Unused << aIPCDoc->SendSelectionEvent(mID, mWidgetID, mType);
    }

    uint64_t  mID;
    uint64_t  mWidgetID;
    uint32_t  mType;
  };

  struct SerializedEvent final : public DeferredEvent
  {
    SerializedEvent(uint64_t aID, uint32_t aType)
      : mID(aID)
      , mType(aType)
    {}

    void Dispatch(DocAccessibleChild* aIPCDoc) override
    {
      Unused << aIPCDoc->SendEvent(mID, mType);
    }

    uint64_t  mID;
    uint32_t  mType;
  };

  mscom::ProxyUniquePtr<IAccessible> mParentProxy;
  nsTArray<UniquePtr<DeferredEvent>> mDeferredEvents;
};

} // namespace a11y
} // namespace mozilla

#endif // mozilla_a11y_DocAccessibleChild_h
