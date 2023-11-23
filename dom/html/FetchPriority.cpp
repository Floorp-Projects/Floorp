/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FetchPriority.h"

#include "mozilla/Assertions.h"
#include "mozilla/Logging.h"
#include "mozilla/dom/RequestBinding.h"
#include "nsISupportsPriority.h"
#include "nsStringFwd.h"

namespace mozilla::dom {
const char* kFetchPriorityAttributeValueHigh = "high";
const char* kFetchPriorityAttributeValueLow = "low";
const char* kFetchPriorityAttributeValueAuto = "auto";

FetchPriority ToFetchPriority(RequestPriority aRequestPriority) {
  switch (aRequestPriority) {
    case RequestPriority::High: {
      return FetchPriority::High;
    }
    case RequestPriority::Low: {
      return FetchPriority::Low;
    }
    case RequestPriority::Auto: {
      return FetchPriority::Auto;
    }
    default: {
      MOZ_ASSERT_UNREACHABLE();
      return FetchPriority::Auto;
    }
  }
}

#ifdef DEBUG
constexpr auto kPriorityHighest = "PRIORITY_HIGHEST"_ns;
constexpr auto kPriorityHigh = "PRIORITY_HIGH"_ns;
constexpr auto kPriorityNormal = "PRIORITY_NORMAL"_ns;
constexpr auto kPriorityLow = "PRIORITY_LOW"_ns;
constexpr auto kPriorityLowest = "PRIORITY_LOWEST"_ns;
constexpr auto kPriorityUnknown = "UNKNOWN"_ns;

/**
 * See <nsISupportsPriority.idl>.
 */
static void SupportsPriorityToString(int32_t aSupportsPriority,
                                     nsACString& aResult) {
  switch (aSupportsPriority) {
    case nsISupportsPriority::PRIORITY_HIGHEST: {
      aResult = kPriorityHighest;
      break;
    }
    case nsISupportsPriority::PRIORITY_HIGH: {
      aResult = kPriorityHigh;
      break;
    }
    case nsISupportsPriority::PRIORITY_NORMAL: {
      aResult = kPriorityNormal;
      break;
    }
    case nsISupportsPriority::PRIORITY_LOW: {
      aResult = kPriorityLow;
      break;
    }
    case nsISupportsPriority::PRIORITY_LOWEST: {
      aResult = kPriorityLowest;
      break;
    }
    default: {
      aResult = kPriorityUnknown;
      break;
    }
  }
}

static const char* ToString(FetchPriority aFetchPriority) {
  switch (aFetchPriority) {
    case FetchPriority::Auto: {
      return kFetchPriorityAttributeValueAuto;
    }
    case FetchPriority::Low: {
      return kFetchPriorityAttributeValueLow;
    }
    case FetchPriority::High: {
      return kFetchPriorityAttributeValueHigh;
    }
    default: {
      MOZ_ASSERT_UNREACHABLE();
      return kFetchPriorityAttributeValueAuto;
    }
  }
}
#endif  // DEBUG

void LogPriorityMapping(LazyLogModule& aLazyLogModule,
                        FetchPriority aFetchPriority,
                        int32_t aSupportsPriority) {
#ifdef DEBUG
  nsDependentCString supportsPriority;
  SupportsPriorityToString(aSupportsPriority, supportsPriority);
  MOZ_LOG(aLazyLogModule, LogLevel::Debug,
          ("Mapping priority: %s -> %s", ToString(aFetchPriority),
           supportsPriority.get()));
#endif  // DEBUG
}
}  // namespace mozilla::dom
