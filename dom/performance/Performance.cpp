/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Performance.h"

#include "GeckoProfiler.h"
#include "nsRFPService.h"
#include "PerformanceEntry.h"
#include "PerformanceMainThread.h"
#include "PerformanceMark.h"
#include "PerformanceMeasure.h"
#include "PerformanceObserver.h"
#include "PerformanceResourceTiming.h"
#include "PerformanceService.h"
#include "PerformanceWorker.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/MessagePortBinding.h"
#include "mozilla/dom/PerformanceBinding.h"
#include "mozilla/dom/PerformanceEntryEvent.h"
#include "mozilla/dom/PerformanceNavigationBinding.h"
#include "mozilla/dom/PerformanceObserverBinding.h"
#include "mozilla/dom/PerformanceNavigationTiming.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/dom/WorkerScope.h"

#define PERFLOG(msg, ...) printf_stderr(msg, ##__VA_ARGS__)

namespace mozilla::dom {

enum class Performance::ResolveTimestampAttribute {
  Start,
  End,
  Duration,
};

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Performance)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_INHERITED(Performance, DOMEventTargetHelper,
                                   mUserEntries, mResourceEntries,
                                   mSecondaryResourceEntries, mObservers);

NS_IMPL_ADDREF_INHERITED(Performance, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(Performance, DOMEventTargetHelper)

/* static */
already_AddRefed<Performance> Performance::CreateForMainThread(
    nsPIDOMWindowInner* aWindow, nsIPrincipal* aPrincipal,
    nsDOMNavigationTiming* aDOMTiming, nsITimedChannel* aChannel) {
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(aWindow->AsGlobal());
  RefPtr<Performance> performance = new PerformanceMainThread(
      aWindow, aDOMTiming, aChannel, aPrincipal->IsSystemPrincipal());
  return performance.forget();
}

/* static */
already_AddRefed<Performance> Performance::CreateForWorker(
    WorkerPrivate* aWorkerPrivate) {
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();

  RefPtr<Performance> performance = new PerformanceWorker(aWorkerPrivate);
  return performance.forget();
}

/* static */
already_AddRefed<Performance> Performance::Get(JSContext* aCx,
                                               nsIGlobalObject* aGlobal) {
  RefPtr<Performance> performance;
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(aGlobal);
  if (window) {
    performance = window->GetPerformance();
  } else {
    const WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
    if (!workerPrivate) {
      return nullptr;
    }

    WorkerGlobalScope* scope = workerPrivate->GlobalScope();
    MOZ_ASSERT(scope);
    performance = scope->GetPerformance();
  }

  return performance.forget();
}

Performance::Performance(nsIGlobalObject* aGlobal, bool aSystemPrincipal)
    : DOMEventTargetHelper(aGlobal),
      mResourceTimingBufferSize(kDefaultResourceTimingBufferSize),
      mPendingNotificationObserversTask(false),
      mPendingResourceTimingBufferFullEvent(false),
      mSystemPrincipal(aSystemPrincipal) {
  MOZ_ASSERT(!NS_IsMainThread());
}

Performance::Performance(nsPIDOMWindowInner* aWindow, bool aSystemPrincipal)
    : DOMEventTargetHelper(aWindow),
      mResourceTimingBufferSize(kDefaultResourceTimingBufferSize),
      mPendingNotificationObserversTask(false),
      mPendingResourceTimingBufferFullEvent(false),
      mSystemPrincipal(aSystemPrincipal) {
  MOZ_ASSERT(NS_IsMainThread());
}

Performance::~Performance() = default;

DOMHighResTimeStamp Performance::TimeStampToDOMHighResForRendering(
    TimeStamp aTimeStamp) const {
  DOMHighResTimeStamp stamp = GetDOMTiming()->TimeStampToDOMHighRes(aTimeStamp);
  if (!IsSystemPrincipal()) {
    // 0 is an inappropriate mixin for this this area; however CSS Animations
    // needs to have it's Time Reduction Logic refactored, so it's currently
    // only clamping for RFP mode. RFP mode gives a much lower time precision,
    // so we accept the security leak here for now.
    return nsRFPService::ReduceTimePrecisionAsMSecsRFPOnly(stamp, 0);
  }
  return stamp;
}

DOMHighResTimeStamp Performance::Now() {
  DOMHighResTimeStamp rawTime = NowUnclamped();

  // XXX: Remove this would cause functions in pkcs11f.h to fail.
  // Bug 1628021 will find out the root cause.
  if (mSystemPrincipal) {
    return rawTime;
  }

  return nsRFPService::ReduceTimePrecisionAsMSecs(
      rawTime, GetRandomTimelineSeed(), mSystemPrincipal,
      CrossOriginIsolated());
}

DOMHighResTimeStamp Performance::NowUnclamped() const {
  TimeDuration duration = TimeStamp::Now() - CreationTimeStamp();
  return duration.ToMilliseconds();
}

DOMHighResTimeStamp Performance::TimeOrigin() {
  if (!mPerformanceService) {
    mPerformanceService = PerformanceService::GetOrCreate();
  }

  MOZ_ASSERT(mPerformanceService);
  DOMHighResTimeStamp rawTimeOrigin =
      mPerformanceService->TimeOrigin(CreationTimeStamp());
  // Time Origin is an absolute timestamp, so we supply a 0 context mix-in
  return nsRFPService::ReduceTimePrecisionAsMSecs(
      rawTimeOrigin, 0, mSystemPrincipal, CrossOriginIsolated());
}

JSObject* Performance::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto) {
  return Performance_Binding::Wrap(aCx, this, aGivenProto);
}

void Performance::GetEntries(nsTArray<RefPtr<PerformanceEntry>>& aRetval) {
  // We return an empty list when 'privacy.resistFingerprinting' is on.
  if (nsContentUtils::ShouldResistFingerprinting()) {
    aRetval.Clear();
    return;
  }

  aRetval = mResourceEntries.Clone();
  aRetval.AppendElements(mUserEntries);
  aRetval.Sort(PerformanceEntryComparator());
}

void Performance::GetEntriesByType(
    const nsAString& aEntryType, nsTArray<RefPtr<PerformanceEntry>>& aRetval) {
  // We return an empty list when 'privacy.resistFingerprinting' is on.
  if (nsContentUtils::ShouldResistFingerprinting()) {
    aRetval.Clear();
    return;
  }

  if (aEntryType.EqualsLiteral("resource")) {
    aRetval = mResourceEntries.Clone();
    return;
  }

  aRetval.Clear();

  if (aEntryType.EqualsLiteral("mark") || aEntryType.EqualsLiteral("measure")) {
    RefPtr<nsAtom> entryType = NS_Atomize(aEntryType);
    for (PerformanceEntry* entry : mUserEntries) {
      if (entry->GetEntryType() == entryType) {
        aRetval.AppendElement(entry);
      }
    }
  }
}

void Performance::GetEntriesByName(
    const nsAString& aName, const Optional<nsAString>& aEntryType,
    nsTArray<RefPtr<PerformanceEntry>>& aRetval) {
  aRetval.Clear();

  // We return an empty list when 'privacy.resistFingerprinting' is on.
  if (nsContentUtils::ShouldResistFingerprinting()) {
    return;
  }

  RefPtr<nsAtom> name = NS_Atomize(aName);
  RefPtr<nsAtom> entryType =
      aEntryType.WasPassed() ? NS_Atomize(aEntryType.Value()) : nullptr;

  if (entryType) {
    if (entryType == nsGkAtoms::mark || entryType == nsGkAtoms::measure) {
      for (PerformanceEntry* entry : mUserEntries) {
        if (entry->GetName() == name && entry->GetEntryType() == entryType) {
          aRetval.AppendElement(entry);
        }
      }
      return;
    }
    if (entryType == nsGkAtoms::resource) {
      for (PerformanceEntry* entry : mResourceEntries) {
        MOZ_ASSERT(entry->GetEntryType() == entryType);
        if (entry->GetName() == name) {
          aRetval.AppendElement(entry);
        }
      }
      return;
    }
    // Invalid entryType
    return;
  }

  nsTArray<PerformanceEntry*> qualifiedResourceEntries;
  nsTArray<PerformanceEntry*> qualifiedUserEntries;
  // ::Measure expects that results from this function are already
  // passed through ReduceTimePrecision. mResourceEntries and mUserEntries
  // are, so the invariant holds.
  for (PerformanceEntry* entry : mResourceEntries) {
    if (entry->GetName() == name) {
      qualifiedResourceEntries.AppendElement(entry);
    }
  }

  for (PerformanceEntry* entry : mUserEntries) {
    if (entry->GetName() == name) {
      qualifiedUserEntries.AppendElement(entry);
    }
  }

  size_t resourceEntriesIdx = 0, userEntriesIdx = 0;
  aRetval.SetCapacity(qualifiedResourceEntries.Length() +
                      qualifiedUserEntries.Length());

  PerformanceEntryComparator comparator;

  while (resourceEntriesIdx < qualifiedResourceEntries.Length() &&
         userEntriesIdx < qualifiedUserEntries.Length()) {
    if (comparator.LessThan(qualifiedResourceEntries[resourceEntriesIdx],
                            qualifiedUserEntries[userEntriesIdx])) {
      aRetval.AppendElement(qualifiedResourceEntries[resourceEntriesIdx]);
      ++resourceEntriesIdx;
    } else {
      aRetval.AppendElement(qualifiedUserEntries[userEntriesIdx]);
      ++userEntriesIdx;
    }
  }

  while (resourceEntriesIdx < qualifiedResourceEntries.Length()) {
    aRetval.AppendElement(qualifiedResourceEntries[resourceEntriesIdx]);
    ++resourceEntriesIdx;
  }

  while (userEntriesIdx < qualifiedUserEntries.Length()) {
    aRetval.AppendElement(qualifiedUserEntries[userEntriesIdx]);
    ++userEntriesIdx;
  }
}

void Performance::GetEntriesByTypeForObserver(
    const nsAString& aEntryType, nsTArray<RefPtr<PerformanceEntry>>& aRetval) {
  GetEntriesByType(aEntryType, aRetval);
}

void Performance::ClearUserEntries(const Optional<nsAString>& aEntryName,
                                   const nsAString& aEntryType) {
  MOZ_ASSERT(!aEntryType.IsEmpty());
  RefPtr<nsAtom> name =
      aEntryName.WasPassed() ? NS_Atomize(aEntryName.Value()) : nullptr;
  RefPtr<nsAtom> entryType = NS_Atomize(aEntryType);
  mUserEntries.RemoveElementsBy([name, entryType](const auto& entry) {
    return (!name || entry->GetName() == name) &&
           (entry->GetEntryType() == entryType);
  });
}

void Performance::ClearResourceTimings() { mResourceEntries.Clear(); }

struct UserTimingMarker {
  static constexpr Span<const char> MarkerTypeName() {
    return MakeStringSpan("UserTiming");
  }
  static void StreamJSONMarkerData(
      baseprofiler::SpliceableJSONWriter& aWriter,
      const ProfilerString16View& aName, bool aIsMeasure,
      const Maybe<ProfilerString16View>& aStartMark,
      const Maybe<ProfilerString16View>& aEndMark) {
    aWriter.StringProperty("name", NS_ConvertUTF16toUTF8(aName));
    if (aIsMeasure) {
      aWriter.StringProperty("entryType", "measure");
    } else {
      aWriter.StringProperty("entryType", "mark");
    }

    if (aStartMark.isSome()) {
      aWriter.StringProperty("startMark", NS_ConvertUTF16toUTF8(*aStartMark));
    } else {
      aWriter.NullProperty("startMark");
    }
    if (aEndMark.isSome()) {
      aWriter.StringProperty("endMark", NS_ConvertUTF16toUTF8(*aEndMark));
    } else {
      aWriter.NullProperty("endMark");
    }
  }
  static MarkerSchema MarkerTypeDisplay() {
    using MS = MarkerSchema;
    MS schema{MS::Location::MarkerChart, MS::Location::MarkerTable};
    schema.SetAllLabels("{marker.data.name}");
    schema.AddStaticLabelValue("Marker", "UserTiming");
    schema.AddKeyLabelFormat("entryType", "Entry Type", MS::Format::String);
    schema.AddKeyLabelFormat("name", "Name", MS::Format::String);
    schema.AddKeyLabelFormat("startMark", "Start Mark", MS::Format::String);
    schema.AddKeyLabelFormat("endMark", "End Mark", MS::Format::String);
    schema.AddStaticLabelValue("Description",
                               "UserTimingMeasure is created using the DOM API "
                               "performance.measure().");
    return schema;
  }
};

already_AddRefed<PerformanceMark> Performance::Mark(
    JSContext* aCx, const nsAString& aName,
    const PerformanceMarkOptions& aMarkOptions, ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> parent = GetParentObject();
  if (!parent || parent->IsDying() || !parent->HasJSGlobal()) {
    aRv.ThrowInvalidStateError("Global object is unavailable");
    return nullptr;
  }

  GlobalObject global(aCx, parent->GetGlobalJSObject());
  if (global.Failed()) {
    aRv.ThrowInvalidStateError("Global object is unavailable");
    return nullptr;
  }

  RefPtr<PerformanceMark> performanceMark =
      PerformanceMark::Constructor(global, aName, aMarkOptions, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // To avoid fingerprinting in User Timing L2, we didn't add marks to the
  // buffer so the user could not get timing data (which can be used to
  // fingerprint) from the API. This may no longer be necessary (since
  // performance.now() has reduced precision to protect against fingerprinting
  // and performance.mark's primary fingerprinting issue is probably this timing
  // data) but we need to do a more thorough reanalysis before we remove the
  // fingerprinting protection. For now, we preserve the User Timing L2 behavior
  // while supporting User Timing L3.
  if (!nsContentUtils::ShouldResistFingerprinting()) {
    InsertUserEntry(performanceMark);
  }

  if (profiler_thread_is_being_profiled_for_markers()) {
    Maybe<uint64_t> innerWindowId;
    if (GetOwner()) {
      innerWindowId = Some(GetOwner()->WindowID());
    }
    profiler_add_marker("UserTiming", geckoprofiler::category::DOM,
                        MarkerInnerWindowId(innerWindowId), UserTimingMarker{},
                        aName, /* aIsMeasure */ false, Nothing{}, Nothing{});
  }

  return performanceMark.forget();
}

void Performance::ClearMarks(const Optional<nsAString>& aName) {
  ClearUserEntries(aName, u"mark"_ns);
}

// To be removed once bug 1124165 lands
bool Performance::IsPerformanceTimingAttribute(const nsAString& aName) const {
  // Note that toJSON is added to this list due to bug 1047848
  static const char* attributes[] = {"navigationStart",
                                     "unloadEventStart",
                                     "unloadEventEnd",
                                     "redirectStart",
                                     "redirectEnd",
                                     "fetchStart",
                                     "domainLookupStart",
                                     "domainLookupEnd",
                                     "connectStart",
                                     "secureConnectionStart",
                                     "connectEnd",
                                     "requestStart",
                                     "responseStart",
                                     "responseEnd",
                                     "domLoading",
                                     "domInteractive",
                                     "domContentLoadedEventStart",
                                     "domContentLoadedEventEnd",
                                     "domComplete",
                                     "loadEventStart",
                                     "loadEventEnd",
                                     nullptr};

  for (uint32_t i = 0; attributes[i]; ++i) {
    if (aName.EqualsASCII(attributes[i])) {
      return true;
    }
  }

  return false;
}

DOMHighResTimeStamp Performance::ConvertMarkToTimestampWithString(
    const nsAString& aName, ErrorResult& aRv) {
  if (IsPerformanceTimingAttribute(aName)) {
    return ConvertNameToTimestamp(aName, aRv);
  }

  AutoTArray<RefPtr<PerformanceEntry>, 1> arr;
  Optional<nsAString> typeParam;
  nsAutoString str;
  str.AssignLiteral("mark");
  typeParam = &str;
  GetEntriesByName(aName, typeParam, arr);
  if (!arr.IsEmpty()) {
    return arr.LastElement()->StartTime();
  }

  nsPrintfCString errorMsg("Given mark name, %s, is unknown",
                           NS_ConvertUTF16toUTF8(aName).get());
  aRv.ThrowSyntaxError(errorMsg);
  return 0;
}

DOMHighResTimeStamp Performance::ConvertMarkToTimestampWithDOMHighResTimeStamp(
    const ResolveTimestampAttribute aAttribute,
    const DOMHighResTimeStamp aTimestamp, ErrorResult& aRv) {
  if (aTimestamp < 0) {
    nsAutoCString attributeName;
    switch (aAttribute) {
      case ResolveTimestampAttribute::Start:
        attributeName = "start";
        break;
      case ResolveTimestampAttribute::End:
        attributeName = "end";
        break;
      case ResolveTimestampAttribute::Duration:
        attributeName = "duration";
        break;
    }

    nsPrintfCString errorMsg("Given attribute %s cannot be negative",
                             attributeName.get());
    aRv.ThrowTypeError(errorMsg);
  }
  return aTimestamp;
}

DOMHighResTimeStamp Performance::ConvertMarkToTimestamp(
    const ResolveTimestampAttribute aAttribute,
    const OwningStringOrDouble& aMarkNameOrTimestamp, ErrorResult& aRv) {
  if (aMarkNameOrTimestamp.IsString()) {
    return ConvertMarkToTimestampWithString(aMarkNameOrTimestamp.GetAsString(),
                                            aRv);
  }

  return ConvertMarkToTimestampWithDOMHighResTimeStamp(
      aAttribute, aMarkNameOrTimestamp.GetAsDouble(), aRv);
}

DOMHighResTimeStamp Performance::ConvertNameToTimestamp(const nsAString& aName,
                                                        ErrorResult& aRv) {
  if (!IsGlobalObjectWindow()) {
    nsPrintfCString errorMsg(
        "Cannot get PerformanceTiming attribute values for non-Window global "
        "object. Given: %s",
        NS_ConvertUTF16toUTF8(aName).get());
    aRv.ThrowTypeError(errorMsg);
    return 0;
  }

  if (aName.EqualsASCII("navigationStart")) {
    return 0;
  }

  // We use GetPerformanceTimingFromString, rather than calling the
  // navigationStart method timing function directly, because the former handles
  // reducing precision against timing attacks.
  const DOMHighResTimeStamp startTime =
      GetPerformanceTimingFromString(u"navigationStart"_ns);
  const DOMHighResTimeStamp endTime = GetPerformanceTimingFromString(aName);
  MOZ_ASSERT(endTime >= 0);
  if (endTime == 0) {
    nsPrintfCString errorMsg(
        "Given PerformanceTiming attribute, %s, isn't available yet",
        NS_ConvertUTF16toUTF8(aName).get());
    aRv.ThrowInvalidAccessError(errorMsg);
    return 0;
  }

  return endTime - startTime;
}

DOMHighResTimeStamp Performance::ResolveEndTimeForMeasure(
    const Optional<nsAString>& aEndMark,
    const Maybe<const PerformanceMeasureOptions&>& aOptions, ErrorResult& aRv) {
  DOMHighResTimeStamp endTime;
  if (aEndMark.WasPassed()) {
    endTime = ConvertMarkToTimestampWithString(aEndMark.Value(), aRv);
  } else if (aOptions && aOptions->mEnd.WasPassed()) {
    endTime = ConvertMarkToTimestamp(ResolveTimestampAttribute::End,
                                     aOptions->mEnd.Value(), aRv);
  } else if (aOptions && aOptions->mStart.WasPassed() &&
             aOptions->mDuration.WasPassed()) {
    const DOMHighResTimeStamp start = ConvertMarkToTimestamp(
        ResolveTimestampAttribute::Start, aOptions->mStart.Value(), aRv);
    if (aRv.Failed()) {
      return 0;
    }

    const DOMHighResTimeStamp duration =
        ConvertMarkToTimestampWithDOMHighResTimeStamp(
            ResolveTimestampAttribute::Duration, aOptions->mDuration.Value(),
            aRv);
    if (aRv.Failed()) {
      return 0;
    }

    endTime = start + duration;
  } else {
    endTime = Now();
  }

  return endTime;
}

DOMHighResTimeStamp Performance::ResolveStartTimeForMeasure(
    const Maybe<const nsAString&>& aStartMark,
    const Maybe<const PerformanceMeasureOptions&>& aOptions, ErrorResult& aRv) {
  DOMHighResTimeStamp startTime;
  if (aOptions && aOptions->mStart.WasPassed()) {
    startTime = ConvertMarkToTimestamp(ResolveTimestampAttribute::Start,
                                       aOptions->mStart.Value(), aRv);
  } else if (aOptions && aOptions->mDuration.WasPassed() &&
             aOptions->mEnd.WasPassed()) {
    const DOMHighResTimeStamp duration =
        ConvertMarkToTimestampWithDOMHighResTimeStamp(
            ResolveTimestampAttribute::Duration, aOptions->mDuration.Value(),
            aRv);
    if (aRv.Failed()) {
      return 0;
    }

    const DOMHighResTimeStamp end = ConvertMarkToTimestamp(
        ResolveTimestampAttribute::End, aOptions->mEnd.Value(), aRv);
    if (aRv.Failed()) {
      return 0;
    }

    startTime = end - duration;
  } else if (aStartMark) {
    startTime = ConvertMarkToTimestampWithString(*aStartMark, aRv);
  } else {
    startTime = 0;
  }

  return startTime;
}

already_AddRefed<PerformanceMeasure> Performance::Measure(
    JSContext* aCx, const nsAString& aName,
    const StringOrPerformanceMeasureOptions& aStartOrMeasureOptions,
    const Optional<nsAString>& aEndMark, ErrorResult& aRv) {
  if (!GetParentObject()) {
    aRv.ThrowInvalidStateError("Global object is unavailable");
    return nullptr;
  }

  // When resisting fingerprinting, we don't add marks to the buffer. Since
  // measure relies on relationships between marks in the buffer, this method
  // will throw if we look for user-entered marks so we return a dummy measure
  // instead of continuing. We could instead return real values for performance
  // timing attributes and dummy values for user-entered marks but this adds
  // complexity that doesn't seem worth the effort because these fingerprinting
  // protections may not longer be necessary (since performance.now() already
  // has reduced precision).
  if (nsContentUtils::ShouldResistFingerprinting()) {
    return do_AddRef(new PerformanceMeasure(GetParentObject(), aName, 0, 0,
                                            JS::NullHandleValue));
  }

  // Maybe is more readable than using the union type directly.
  Maybe<const PerformanceMeasureOptions&> options;
  if (aStartOrMeasureOptions.IsPerformanceMeasureOptions()) {
    options.emplace(aStartOrMeasureOptions.GetAsPerformanceMeasureOptions());
  }

  const bool isOptionsNotEmpty =
      options.isSome() &&
      (!options->mDetail.isUndefined() || options->mStart.WasPassed() ||
       options->mEnd.WasPassed() || options->mDuration.WasPassed());
  if (isOptionsNotEmpty) {
    if (aEndMark.WasPassed()) {
      aRv.ThrowTypeError(
          "Cannot provide separate endMark argument if "
          "PerformanceMeasureOptions argument is given");
      return nullptr;
    }

    if (!options->mStart.WasPassed() && !options->mEnd.WasPassed()) {
      aRv.ThrowTypeError(
          "PerformanceMeasureOptions must have start and/or end member");
      return nullptr;
    }

    if (options->mStart.WasPassed() && options->mDuration.WasPassed() &&
        options->mEnd.WasPassed()) {
      aRv.ThrowTypeError(
          "PerformanceMeasureOptions cannot have all of the following members: "
          "start, duration, and end");
      return nullptr;
    }
  }

  const DOMHighResTimeStamp endTime =
      ResolveEndTimeForMeasure(aEndMark, options, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // Convert to Maybe for consistency with options.
  Maybe<const nsAString&> startMark;
  if (aStartOrMeasureOptions.IsString()) {
    startMark.emplace(aStartOrMeasureOptions.GetAsString());
  }
  const DOMHighResTimeStamp startTime =
      ResolveStartTimeForMeasure(startMark, options, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  JS::Rooted<JS::Value> detail(aCx);
  if (options && !options->mDetail.isNullOrUndefined()) {
    StructuredSerializeOptions serializeOptions;
    JS::Rooted<JS::Value> valueToClone(aCx, options->mDetail);
    nsContentUtils::StructuredClone(aCx, GetParentObject(), valueToClone,
                                    serializeOptions, &detail, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
  } else {
    detail.setNull();
  }

  RefPtr<PerformanceMeasure> performanceMeasure = new PerformanceMeasure(
      GetParentObject(), aName, startTime, endTime, detail);
  InsertUserEntry(performanceMeasure);

  if (profiler_thread_is_being_profiled_for_markers()) {
    TimeStamp startTimeStamp =
        CreationTimeStamp() + TimeDuration::FromMilliseconds(startTime);
    TimeStamp endTimeStamp =
        CreationTimeStamp() + TimeDuration::FromMilliseconds(endTime);

    Maybe<nsString> endMark;
    if (aEndMark.WasPassed()) {
      endMark.emplace(aEndMark.Value());
    }

    Maybe<uint64_t> innerWindowId;
    if (GetOwner()) {
      innerWindowId = Some(GetOwner()->WindowID());
    }
    profiler_add_marker("UserTiming", geckoprofiler::category::DOM,
                        {MarkerTiming::Interval(startTimeStamp, endTimeStamp),
                         MarkerInnerWindowId(innerWindowId)},
                        UserTimingMarker{}, aName, /* aIsMeasure */ true,
                        startMark, endMark);
  }

  return performanceMeasure.forget();
}

void Performance::ClearMeasures(const Optional<nsAString>& aName) {
  ClearUserEntries(aName, u"measure"_ns);
}

void Performance::LogEntry(PerformanceEntry* aEntry,
                           const nsACString& aOwner) const {
  PERFLOG("Performance Entry: %s|%s|%s|%f|%f|%" PRIu64 "\n",
          aOwner.BeginReading(),
          NS_ConvertUTF16toUTF8(aEntry->GetEntryType()->GetUTF16String()).get(),
          NS_ConvertUTF16toUTF8(aEntry->GetName()->GetUTF16String()).get(),
          aEntry->StartTime(), aEntry->Duration(),
          static_cast<uint64_t>(PR_Now() / PR_USEC_PER_MSEC));
}

void Performance::TimingNotification(PerformanceEntry* aEntry,
                                     const nsACString& aOwner,
                                     const double aEpoch) {
  PerformanceEntryEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  aEntry->GetName(init.mName);
  aEntry->GetEntryType(init.mEntryType);
  init.mStartTime = aEntry->StartTime();
  init.mDuration = aEntry->Duration();
  init.mEpoch = aEpoch;
  CopyUTF8toUTF16(aOwner, init.mOrigin);

  RefPtr<PerformanceEntryEvent> perfEntryEvent =
      PerformanceEntryEvent::Constructor(this, u"performanceentry"_ns, init);

  nsCOMPtr<EventTarget> et = do_QueryInterface(GetOwner());
  if (et) {
    et->DispatchEvent(*perfEntryEvent);
  }
}

void Performance::InsertUserEntry(PerformanceEntry* aEntry) {
  mUserEntries.InsertElementSorted(aEntry, PerformanceEntryComparator());

  QueueEntry(aEntry);
}

/*
 * Steps are labeled according to the description found at
 * https://w3c.github.io/resource-timing/#sec-extensions-performance-interface.
 *
 * Buffer Full Event
 */
void Performance::BufferEvent() {
  /*
   * While resource timing secondary buffer is not empty,
   * run the following substeps:
   */
  while (!mSecondaryResourceEntries.IsEmpty()) {
    uint32_t secondaryResourceEntriesBeforeCount = 0;
    uint32_t secondaryResourceEntriesAfterCount = 0;

    /*
     * Let number of excess entries before be resource
     * timing secondary buffer current size.
     */
    secondaryResourceEntriesBeforeCount = mSecondaryResourceEntries.Length();

    /*
     * If can add resource timing entry returns false,
     * then fire an event named resourcetimingbufferfull
     * at the Performance object.
     */
    if (!CanAddResourceTimingEntry()) {
      DispatchBufferFullEvent();
    }

    /*
     * Run copy secondary buffer.
     *
     * While resource timing secondary buffer is not
     * empty and can add resource timing entry returns
     * true ...
     */
    while (!mSecondaryResourceEntries.IsEmpty() &&
           CanAddResourceTimingEntry()) {
      /*
       * Let entry be the oldest PerformanceResourceTiming
       * in resource timing secondary buffer. Add entry to
       * the end of performance entry buffer. Increment
       * resource timing buffer current size by 1.
       */
      mResourceEntries.InsertElementSorted(
          mSecondaryResourceEntries.ElementAt(0), PerformanceEntryComparator());
      /*
       * Remove entry from resource timing secondary buffer.
       * Decrement resource timing secondary buffer current
       * size by 1.
       */
      mSecondaryResourceEntries.RemoveElementAt(0);
    }

    /*
     * Let number of excess entries after be resource
     * timing secondary buffer current size.
     */
    secondaryResourceEntriesAfterCount = mSecondaryResourceEntries.Length();

    /*
     * If number of excess entries before is lower than
     * or equals number of excess entries after, then
     * remove all entries from resource timing secondary
     * buffer, set resource timing secondary buffer current
     * size to 0, and abort these steps.
     */
    if (secondaryResourceEntriesBeforeCount <=
        secondaryResourceEntriesAfterCount) {
      mSecondaryResourceEntries.Clear();
      break;
    }
  }
  /*
   * Set resource timing buffer full event pending flag
   * to false.
   */
  mPendingResourceTimingBufferFullEvent = false;
}

void Performance::SetResourceTimingBufferSize(uint64_t aMaxSize) {
  mResourceTimingBufferSize = aMaxSize;
}

/*
 * Steps are labeled according to the description found at
 * https://w3c.github.io/resource-timing/#sec-extensions-performance-interface.
 *
 * Can Add Resource Timing Entry
 */
MOZ_ALWAYS_INLINE bool Performance::CanAddResourceTimingEntry() {
  /*
   * If resource timing buffer current size is smaller than resource timing
   * buffer size limit, return true. [Otherwise,] [r]eturn false.
   */
  return mResourceEntries.Length() < mResourceTimingBufferSize;
}

/*
 * Steps are labeled according to the description found at
 * https://w3c.github.io/resource-timing/#sec-extensions-performance-interface.
 *
 * Add a PerformanceResourceTiming Entry
 */
void Performance::InsertResourceEntry(PerformanceEntry* aEntry) {
  MOZ_ASSERT(aEntry);

  if (nsContentUtils::ShouldResistFingerprinting()) {
    return;
  }

  /*
   * Let new entry be the input PerformanceEntry to be added.
   *
   * If can add resource timing entry returns true and resource
   * timing buffer full event pending flag is false ...
   */
  if (CanAddResourceTimingEntry() && !mPendingResourceTimingBufferFullEvent) {
    /*
     * Add new entry to the performance entry buffer.
     * Increase resource timing buffer current size by 1.
     */
    mResourceEntries.InsertElementSorted(aEntry, PerformanceEntryComparator());
    QueueEntry(aEntry);
    return;
  }

  /*
   * If resource timing buffer full event pending flag is
   * false ...
   */
  if (!mPendingResourceTimingBufferFullEvent) {
    /*
     * Set resource timing buffer full event pending flag
     * to true.
     */
    mPendingResourceTimingBufferFullEvent = true;

    /*
     * Queue a task to run fire a buffer full event.
     */
    NS_DispatchToCurrentThread(NewCancelableRunnableMethod(
        "Performance::BufferEvent", this, &Performance::BufferEvent));
  }
  /*
   * Add new entry to the resource timing secondary buffer.
   * Increase resource timing secondary buffer current size
   * by 1.
   */
  mSecondaryResourceEntries.InsertElementSorted(aEntry,
                                                PerformanceEntryComparator());
}

void Performance::AddObserver(PerformanceObserver* aObserver) {
  mObservers.AppendElementUnlessExists(aObserver);
}

void Performance::RemoveObserver(PerformanceObserver* aObserver) {
  mObservers.RemoveElement(aObserver);
}

void Performance::NotifyObservers() {
  mPendingNotificationObserversTask = false;
  NS_OBSERVER_ARRAY_NOTIFY_XPCOM_OBSERVERS(mObservers, Notify, ());
}

void Performance::CancelNotificationObservers() {
  mPendingNotificationObserversTask = false;
}

class NotifyObserversTask final : public CancelableRunnable {
 public:
  explicit NotifyObserversTask(Performance* aPerformance)
      : CancelableRunnable("dom::NotifyObserversTask"),
        mPerformance(aPerformance) {
    MOZ_ASSERT(mPerformance);
  }

  // MOZ_CAN_RUN_SCRIPT_BOUNDARY for now until Runnable::Run is
  // MOZ_CAN_RUN_SCRIPT.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  NS_IMETHOD Run() override {
    MOZ_ASSERT(mPerformance);
    RefPtr<Performance> performance(mPerformance);
    performance->NotifyObservers();
    return NS_OK;
  }

  nsresult Cancel() override {
    mPerformance->CancelNotificationObservers();
    mPerformance = nullptr;
    return NS_OK;
  }

 private:
  ~NotifyObserversTask() = default;

  RefPtr<Performance> mPerformance;
};

void Performance::QueueNotificationObserversTask() {
  if (!mPendingNotificationObserversTask) {
    RunNotificationObserversTask();
  }
}

void Performance::RunNotificationObserversTask() {
  mPendingNotificationObserversTask = true;
  nsCOMPtr<nsIRunnable> task = new NotifyObserversTask(this);
  nsresult rv;
  if (GetOwnerGlobal()) {
    rv = GetOwnerGlobal()->Dispatch(TaskCategory::Other, task.forget());
  } else {
    rv = NS_DispatchToCurrentThread(task);
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mPendingNotificationObserversTask = false;
  }
}

void Performance::QueueEntry(PerformanceEntry* aEntry) {
  nsTObserverArray<PerformanceObserver*> interestedObservers;
  if (!mObservers.IsEmpty()) {
    const auto [begin, end] = mObservers.NonObservingRange();
    std::copy_if(begin, end, MakeBackInserter(interestedObservers),
                 [aEntry](PerformanceObserver* observer) {
                   return observer->ObservesTypeOfEntry(aEntry);
                 });
  }

  NS_OBSERVER_ARRAY_NOTIFY_XPCOM_OBSERVERS(interestedObservers, QueueEntry,
                                           (aEntry));

  aEntry->BufferEntryIfNeeded();

  if (!interestedObservers.IsEmpty()) {
    QueueNotificationObserversTask();
  }
}

void Performance::MemoryPressure() { mUserEntries.Clear(); }

size_t Performance::SizeOfUserEntries(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  size_t userEntries = 0;
  for (const PerformanceEntry* entry : mUserEntries) {
    userEntries += entry->SizeOfIncludingThis(aMallocSizeOf);
  }
  return userEntries;
}

size_t Performance::SizeOfResourceEntries(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  size_t resourceEntries = 0;
  for (const PerformanceEntry* entry : mResourceEntries) {
    resourceEntries += entry->SizeOfIncludingThis(aMallocSizeOf);
  }
  return resourceEntries;
}

}  // namespace mozilla::dom
