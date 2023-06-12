/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_SCOPEDLOGEXTRAINFO_H_
#define DOM_QUOTA_SCOPEDLOGEXTRAINFO_H_

#include "mozilla/dom/quota/Config.h"

#include <map>
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/ThreadLocal.h"
#include "nsString.h"
#include "nsXULAppAPI.h"

namespace mozilla::dom::quota {

struct MOZ_STACK_CLASS ScopedLogExtraInfo {
  static constexpr const char kTagQuery[] = "query";
  static constexpr const char kTagContext[] = "context";

#ifdef QM_SCOPED_LOG_EXTRA_INFO_ENABLED
 private:
  static auto FindSlot(const char* aTag);

 public:
  template <size_t N>
  ScopedLogExtraInfo(const char (&aTag)[N], const nsACString& aExtraInfo)
      : mTag{aTag}, mPreviousValue(nullptr), mCurrentValue{aExtraInfo} {
    AddInfo();
  }

  ~ScopedLogExtraInfo();

  ScopedLogExtraInfo(ScopedLogExtraInfo&& aOther) noexcept;
  ScopedLogExtraInfo& operator=(ScopedLogExtraInfo&& aOther) = delete;

  ScopedLogExtraInfo(const ScopedLogExtraInfo&) = delete;
  ScopedLogExtraInfo& operator=(const ScopedLogExtraInfo&) = delete;

  using ScopedLogExtraInfoMap = std::map<const char*, const nsACString*>;
  static ScopedLogExtraInfoMap GetExtraInfoMap();

  static void Initialize();

 private:
  const char* mTag;
  const nsACString* mPreviousValue;
  nsCString mCurrentValue;

  static MOZ_THREAD_LOCAL(const nsACString*) sQueryValue;
  static MOZ_THREAD_LOCAL(const nsACString*) sContextValue;

  void AddInfo();
#else
  template <size_t N>
  ScopedLogExtraInfo(const char (&aTag)[N], const nsACString& aExtraInfo) {}

  // user-defined to silence unused variable warnings
  ~ScopedLogExtraInfo() {}
#endif
};

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_SCOPEDLOGEXTRAINFO_H_
