/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_WorkerError_h
#define mozilla_dom_workers_WorkerError_h

#include "mozilla/dom/SerializedStackHolder.h"
#include "mozilla/dom/WorkerCommon.h"
#include "jsapi.h"

namespace mozilla {

class DOMEventTargetHelper;

namespace dom {

class ErrorData;
class WorkerErrorBase {
 public:
  nsString mMessage;
  nsString mFilename;
  uint32_t mLineNumber;
  uint32_t mColumnNumber;
  uint32_t mErrorNumber;

  WorkerErrorBase() : mLineNumber(0), mColumnNumber(0), mErrorNumber(0) {}

  void AssignErrorBase(JSErrorBase* aReport);
};

class WorkerErrorNote : public WorkerErrorBase {
 public:
  void AssignErrorNote(JSErrorNotes::Note* aNote);
};

class WorkerPrivate;

class WorkerErrorReport : public WorkerErrorBase, public SerializedStackHolder {
 public:
  nsString mLine;
  uint32_t mFlags;
  JSExnType mExnType;
  bool mMutedError;
  nsTArray<WorkerErrorNote> mNotes;

  WorkerErrorReport();

  void AssignErrorReport(JSErrorReport* aReport);

  // aWorkerPrivate is the worker thread we're on (or the main thread, if null)
  // aTarget is the worker object that we are going to fire an error at
  // (if any).
  static void ReportError(
      JSContext* aCx, WorkerPrivate* aWorkerPrivate, bool aFireAtScope,
      DOMEventTargetHelper* aTarget, UniquePtr<WorkerErrorReport> aReport,
      uint64_t aInnerWindowId,
      JS::Handle<JS::Value> aException = JS::NullHandleValue);

  static void LogErrorToConsole(JSContext* aCx, WorkerErrorReport& aReport,
                                uint64_t aInnerWindowId);

  static void LogErrorToConsole(const mozilla::dom::ErrorData& aReport,
                                uint64_t aInnerWindowId,
                                JS::HandleObject aStack = nullptr,
                                JS::HandleObject aStackGlobal = nullptr);

  static void CreateAndDispatchGenericErrorRunnableToParent(
      WorkerPrivate* aWorkerPrivate);
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_workers_WorkerError_h
