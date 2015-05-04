/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cellbroadcast_CellBroadcastIPCService_h
#define mozilla_dom_cellbroadcast_CellBroadcastIPCService_h

#include "mozilla/dom/cellbroadcast/PCellBroadcastChild.h"
#include "nsICellBroadcastService.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {
namespace cellbroadcast {

class CellBroadcastIPCService final : public PCellBroadcastChild
                                    , public nsICellBroadcastService

{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICELLBROADCASTSERVICE

  CellBroadcastIPCService();

  // PCellBroadcastChild interface
  virtual bool
  RecvNotifyReceivedMessage(const uint32_t& aServiceId,
                            const uint32_t& aGsmGeographicalScope,
                            const uint16_t& aMessageCode,
                            const uint16_t& aMessageId,
                            const nsString& aLanguage,
                            const nsString& aBody,
                            const uint32_t& aMessageClass,
                            const uint64_t& aTimestamp,
                            const uint32_t& aCdmaServiceCategory,
                            const bool& aHasEtwsInfo,
                            const uint32_t& aEtwsWarningType,
                            const bool& aEtwsEmergencyUserAlert,
                            const bool& aEtwsPopup) override;

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

private:
  // final suppresses -Werror,-Wdelete-non-virtual-dtor
  ~CellBroadcastIPCService();

  bool mActorDestroyed;
  nsTArray<nsCOMPtr<nsICellBroadcastListener>> mListeners;
};

} // namespace cellbroadcast
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cellbroadcast_CellBroadcastIPCService_h
