/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CheckerboardReportService.h"

#include "gfxPrefs.h" // for gfxPrefs
#include "jsapi.h" // for JS_Now
#include "MainThreadUtils.h" // for NS_IsMainThread
#include "mozilla/Assertions.h" // for MOZ_ASSERT
#include "mozilla/ClearOnShutdown.h" // for ClearOnShutdown
#include "mozilla/dom/CheckerboardReportServiceBinding.h" // for dom::CheckerboardReports
#include "nsContentUtils.h" // for nsContentUtils
#include "nsXULAppAPI.h"

namespace mozilla {
namespace layers {

/*static*/ StaticRefPtr<CheckerboardEventStorage> CheckerboardEventStorage::sInstance;

/*static*/ already_AddRefed<CheckerboardEventStorage>
CheckerboardEventStorage::GetInstance()
{
  // The instance in the parent process does all the work, so if this is getting
  // called in the child process something is likely wrong.
  MOZ_ASSERT(XRE_IsParentProcess());

  MOZ_ASSERT(NS_IsMainThread());
  if (!sInstance) {
    sInstance = new CheckerboardEventStorage();
    ClearOnShutdown(&sInstance);
  }
  RefPtr<CheckerboardEventStorage> instance = sInstance.get();
  return instance.forget();
}

void
CheckerboardEventStorage::ReportCheckerboard(uint32_t aSeverity, const std::string& aLog)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (aSeverity == 0) {
    // This code assumes all checkerboard reports have a nonzero severity.
    return;
  }

  CheckerboardReport severe(aSeverity, JS_Now(), aLog);
  CheckerboardReport recent;

  // First look in the "severe" reports to see if the new one belongs in that
  // list.
  for (int i = 0; i < SEVERITY_MAX_INDEX; i++) {
    if (mCheckerboardReports[i].mSeverity >= severe.mSeverity) {
      continue;
    }
    // The new one deserves to be in the "severe" list. Take the one getting
    // bumped off the list, and put it in |recent| for possible insertion into
    // the recents list.
    recent = mCheckerboardReports[SEVERITY_MAX_INDEX - 1];

    // Shuffle the severe list down, insert the new one.
    for (int j = SEVERITY_MAX_INDEX - 1; j > i; j--) {
      mCheckerboardReports[j] = mCheckerboardReports[j - 1];
    }
    mCheckerboardReports[i] = severe;
    severe.mSeverity = 0; // mark |severe| as inserted
    break;
  }

  // If |severe.mSeverity| is nonzero, the incoming report didn't get inserted
  // into the severe list; put it into |recent| for insertion into the recent
  // list.
  if (severe.mSeverity) {
    MOZ_ASSERT(recent.mSeverity == 0, "recent should be empty here");
    recent = severe;
  } // else |recent| may hold a report that got knocked out of the severe list.

  if (recent.mSeverity == 0) {
    // Nothing to be inserted into the recent list.
    return;
  }

  // If it wasn't in the "severe" list, add it to the "recent" list.
  for (int i = SEVERITY_MAX_INDEX; i < RECENT_MAX_INDEX; i++) {
    if (mCheckerboardReports[i].mTimestamp >= recent.mTimestamp) {
      continue;
    }
    // |recent| needs to be inserted at |i|. Shuffle the remaining ones down
    // and insert it.
    for (int j = RECENT_MAX_INDEX - 1; j > i; j--) {
      mCheckerboardReports[j] = mCheckerboardReports[j - 1];
    }
    mCheckerboardReports[i] = recent;
    break;
  }
}

void
CheckerboardEventStorage::GetReports(nsTArray<dom::CheckerboardReport>& aOutReports)
{
  MOZ_ASSERT(NS_IsMainThread());

  for (int i = 0; i < RECENT_MAX_INDEX; i++) {
    CheckerboardReport& r = mCheckerboardReports[i];
    if (r.mSeverity == 0) {
      continue;
    }
    dom::CheckerboardReport report;
    report.mSeverity.Construct() = r.mSeverity;
    report.mTimestamp.Construct() = r.mTimestamp / 1000; // micros to millis
    report.mLog.Construct() = NS_ConvertUTF8toUTF16(r.mLog.c_str(), r.mLog.size());
    report.mReason.Construct() = (i < SEVERITY_MAX_INDEX)
        ? dom::CheckerboardReason::Severe
        : dom::CheckerboardReason::Recent;
    aOutReports.AppendElement(report);
  }
}

} // namespace layers

namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(CheckerboardReportService, mParent)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(CheckerboardReportService, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(CheckerboardReportService, Release)

/*static*/ bool
CheckerboardReportService::IsEnabled(JSContext* aCtx, JSObject* aGlobal)
{
  // Only allow this in the parent process
  if (!XRE_IsParentProcess()) {
    return false;
  }
  // Allow privileged code or about:checkerboard (unprivileged) to access this.
  return nsContentUtils::IsCallerChrome()
      || nsContentUtils::IsSpecificAboutPage(aGlobal, "about:checkerboard");
}

/*static*/ already_AddRefed<CheckerboardReportService>
CheckerboardReportService::Constructor(const dom::GlobalObject& aGlobal, ErrorResult& aRv)
{
  RefPtr<CheckerboardReportService> ces = new CheckerboardReportService(aGlobal.GetAsSupports());
  return ces.forget();
}

CheckerboardReportService::CheckerboardReportService(nsISupports* aParent)
  : mParent(aParent)
{
}

JSObject*
CheckerboardReportService::WrapObject(JSContext* aCtx, JS::Handle<JSObject*> aGivenProto)
{
  return CheckerboardReportServiceBinding::Wrap(aCtx, this, aGivenProto);
}

nsISupports*
CheckerboardReportService::GetParentObject()
{
  return mParent;
}

void
CheckerboardReportService::GetReports(nsTArray<dom::CheckerboardReport>& aOutReports)
{
  RefPtr<mozilla::layers::CheckerboardEventStorage> instance =
      mozilla::layers::CheckerboardEventStorage::GetInstance();
  MOZ_ASSERT(instance);
  instance->GetReports(aOutReports);
}

bool
CheckerboardReportService::IsRecordingEnabled() const
{
  return gfxPrefs::APZRecordCheckerboarding();
}

void
CheckerboardReportService::SetRecordingEnabled(bool aEnabled)
{
  gfxPrefs::SetAPZRecordCheckerboarding(aEnabled);
}

} // namespace dom
} // namespace mozilla
