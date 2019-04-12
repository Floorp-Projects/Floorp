/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CheckerboardReportService_h
#define mozilla_dom_CheckerboardReportService_h

#include <string>

#include "js/TypeDecls.h"         // for JSContext, JSObject
#include "mozilla/ErrorResult.h"  // for ErrorResult
#include "mozilla/StaticPtr.h"    // for StaticRefPtr
#include "nsCOMPtr.h"             // for nsCOMPtr
#include "nsISupports.h"          // for NS_INLINE_DECL_REFCOUNTING
#include "nsWrapperCache.h"       // for nsWrapperCache

namespace mozilla {

namespace dom {
struct CheckerboardReport;
}

namespace layers {

// CheckerboardEventStorage is a singleton that stores info on checkerboard
// events, so that they can be accessed from about:checkerboard and visualized.
// Note that this class is NOT threadsafe, and all methods must be called on
// the main thread.
class CheckerboardEventStorage {
  NS_INLINE_DECL_REFCOUNTING(CheckerboardEventStorage)

 public:
  /**
   * Get the singleton instance.
   */
  static already_AddRefed<CheckerboardEventStorage> GetInstance();

  /**
   * Get the stored checkerboard reports.
   */
  void GetReports(nsTArray<dom::CheckerboardReport>& aOutReports);

  /**
   * Save a checkerboard event log, optionally dropping older ones that were
   * less severe or less recent. Zero-severity reports may be ignored entirely.
   */
  static void Report(uint32_t aSeverity, const std::string& aLog);

 private:
  /* Stuff for refcounted singleton */
  CheckerboardEventStorage() {}
  virtual ~CheckerboardEventStorage() = default;

  static StaticRefPtr<CheckerboardEventStorage> sInstance;

  void ReportCheckerboard(uint32_t aSeverity, const std::string& aLog);

 private:
  /**
   * Struct that this class uses internally to store a checkerboard report.
   */
  struct CheckerboardReport {
    uint32_t mSeverity;  // if 0, this report is empty
    int64_t mTimestamp;  // microseconds since epoch, as from JS_Now()
    std::string mLog;

    CheckerboardReport() : mSeverity(0), mTimestamp(0) {}

    CheckerboardReport(uint32_t aSeverity, int64_t aTimestamp,
                       const std::string& aLog)
        : mSeverity(aSeverity), mTimestamp(aTimestamp), mLog(aLog) {}
  };

  // The first 5 (indices 0-4) are the most severe ones in decreasing order
  // of severity; the next 5 (indices 5-9) are the most recent ones that are
  // not already in the "severe" list.
  static const int SEVERITY_MAX_INDEX = 5;
  static const int RECENT_MAX_INDEX = 10;
  CheckerboardReport mCheckerboardReports[RECENT_MAX_INDEX];
};

}  // namespace layers

namespace dom {

class GlobalObject;

/**
 * CheckerboardReportService is a wrapper object that allows access to the
 * stuff in CheckerboardEventStorage (above). We need this wrapper for proper
 * garbage/cycle collection, since this can be accessed from JS.
 */
class CheckerboardReportService : public nsWrapperCache {
 public:
  /**
   * Check if the given page is allowed to access this object via the WebIDL
   * bindings. It only returns true if the page is about:checkerboard.
   */
  static bool IsEnabled(JSContext* aCtx, JSObject* aGlobal);

  /*
   * Other standard WebIDL binding glue.
   */

  static already_AddRefed<CheckerboardReportService> Constructor(
      const dom::GlobalObject& aGlobal, ErrorResult& aRv);

  explicit CheckerboardReportService(nsISupports* aSupports);

  JSObject* WrapObject(JSContext* aCtx,
                       JS::Handle<JSObject*> aGivenProto) override;

  nsISupports* GetParentObject();

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(CheckerboardReportService)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(CheckerboardReportService)

 public:
  /*
   * The methods exposed via the webidl.
   */
  void GetReports(nsTArray<dom::CheckerboardReport>& aOutReports);
  bool IsRecordingEnabled() const;
  void SetRecordingEnabled(bool aEnabled);
  void FlushActiveReports();

 private:
  virtual ~CheckerboardReportService() = default;

  nsCOMPtr<nsISupports> mParent;
};

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_layers_CheckerboardReportService_h */
