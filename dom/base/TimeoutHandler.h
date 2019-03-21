/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_timeout_handler_h
#define mozilla_dom_timeout_handler_h

#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsITimeoutHandler.h"
#include "nsCycleCollectionParticipant.h"
#include "nsString.h"
#include "mozilla/Attributes.h"

namespace mozilla {
namespace dom {

/**
 * Utility class for implementing nsITimeoutHandlers, designed to be subclassed.
 */
class TimeoutHandler : public nsITimeoutHandler {
 public:
  // TimeoutHandler doesn't actually contain cycles, but subclasses
  // probably will.
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(TimeoutHandler)

  MOZ_CAN_RUN_SCRIPT virtual void Call() override;
  virtual void GetLocation(const char** aFileName, uint32_t* aLineNo,
                           uint32_t* aColumn) override;
  virtual void MarkForCC() override {}

 protected:
  TimeoutHandler() : mFileName(""), mLineNo(0), mColumn(0) {}
  explicit TimeoutHandler(JSContext* aCx);

  virtual ~TimeoutHandler() {}

 private:
  TimeoutHandler(const TimeoutHandler&) = delete;
  TimeoutHandler& operator=(const TimeoutHandler&) = delete;
  TimeoutHandler& operator=(const TimeoutHandler&&) = delete;

  nsCString mFileName;
  uint32_t mLineNo;
  uint32_t mColumn;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_timeout_handler_h
