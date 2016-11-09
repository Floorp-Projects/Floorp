/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocAccessibleChild.h"

#include "Accessible-inl.h"
#include "mozilla/a11y/PlatformChild.h"
#include "mozilla/ClearOnShutdown.h"

namespace mozilla {
namespace a11y {

static StaticAutoPtr<PlatformChild> sPlatformChild;

DocAccessibleChild::DocAccessibleChild(DocAccessible* aDoc)
  : DocAccessibleChildBase(aDoc)
{
  MOZ_COUNT_CTOR_INHERITED(DocAccessibleChild, DocAccessibleChildBase);
  if (!sPlatformChild) {
    sPlatformChild = new PlatformChild();
    ClearOnShutdown(&sPlatformChild, ShutdownPhase::Shutdown);
  }
}

DocAccessibleChild::~DocAccessibleChild()
{
  MOZ_COUNT_DTOR_INHERITED(DocAccessibleChild, DocAccessibleChildBase);
}

bool
DocAccessibleChild::RecvParentCOMProxy(const IAccessibleHolder& aParentCOMProxy)
{
  MOZ_ASSERT(!mParentProxy && !aParentCOMProxy.IsNull());
  mParentProxy.reset(const_cast<IAccessibleHolder&>(aParentCOMProxy).Release());

  for (uint32_t i = 0, l = mDeferredEvents.Length(); i < l; ++i) {
    mDeferredEvents[i]->Dispatch(this);
  }

  mDeferredEvents.Clear();

  return true;
}

bool
DocAccessibleChild::SendEvent(const uint64_t& aID, const uint32_t& aType)
{
  if (mParentProxy) {
    return PDocAccessibleChild::SendEvent(aID, aType);
  }

  mDeferredEvents.AppendElement(MakeUnique<SerializedEvent>(aID, aType));
  return false;
}

void
DocAccessibleChild::MaybeSendShowEvent(ShowEventData& aData, bool aFromUser)
{
  if (mParentProxy) {
    Unused << SendShowEvent(aData, aFromUser);
    return;
  }

  mDeferredEvents.AppendElement(MakeUnique<SerializedShow>(aData, aFromUser));
}

bool
DocAccessibleChild::SendHideEvent(const uint64_t& aRootID,
                                  const bool& aFromUser)
{
  if (mParentProxy) {
    return PDocAccessibleChild::SendHideEvent(aRootID, aFromUser);
  }

  mDeferredEvents.AppendElement(MakeUnique<SerializedHide>(aRootID, aFromUser));
  return true;
}

bool
DocAccessibleChild::SendStateChangeEvent(const uint64_t& aID,
                                         const uint64_t& aState,
                                         const bool& aEnabled)
{
  if (mParentProxy) {
    return PDocAccessibleChild::SendStateChangeEvent(aID, aState, aEnabled);
  }

  mDeferredEvents.AppendElement(MakeUnique<SerializedStateChange>(aID, aState,
                                                                  aEnabled));
  return true;
}

bool
DocAccessibleChild::SendCaretMoveEvent(const uint64_t& aID,
                                       const int32_t& aOffset)
{
  if (mParentProxy) {
    return PDocAccessibleChild::SendCaretMoveEvent(aID, aOffset);
  }

  mDeferredEvents.AppendElement(MakeUnique<SerializedCaretMove>(aID, aOffset));
  return true;
}

bool
DocAccessibleChild::SendTextChangeEvent(const uint64_t& aID,
                                        const nsString& aStr,
                                        const int32_t& aStart,
                                        const uint32_t& aLen,
                                        const bool& aIsInsert,
                                        const bool& aFromUser)
{
  if (mParentProxy) {
    return PDocAccessibleChild::SendTextChangeEvent(aID, aStr, aStart,
                                                    aLen, aIsInsert, aFromUser);
  }

  mDeferredEvents.AppendElement(MakeUnique<SerializedTextChange>(aID, aStr,
                                                                 aStart, aLen,
                                                                 aIsInsert,
                                                                 aFromUser));
  return true;
}

bool
DocAccessibleChild::SendSelectionEvent(const uint64_t& aID,
                                       const uint64_t& aWidgetID,
                                       const uint32_t& aType)
{
  if (mParentProxy) {
    return PDocAccessibleChild::SendSelectionEvent(aID, aWidgetID, aType);
  }

  mDeferredEvents.AppendElement(MakeUnique<SerializedSelection>(aID, aWidgetID,
                                                                aType));
  return true;
}


} // namespace a11y
} // namespace mozilla

