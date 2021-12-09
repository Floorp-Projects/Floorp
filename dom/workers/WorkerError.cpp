/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerError.h"

#include <stdio.h>
#include <algorithm>
#include <utility>
#include "MainThreadUtils.h"
#include "WorkerPrivate.h"
#include "WorkerRunnable.h"
#include "WorkerScope.h"
#include "js/ComparisonOperators.h"
#include "js/UniquePtr.h"
#include "js/friend/ErrorMessages.h"
#include "jsapi.h"
#include "mozilla/ArrayAlgorithm.h"
#include "mozilla/ArrayIterator.h"
#include "mozilla/Assertions.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Span.h"
#include "mozilla/ThreadSafeWeakPtr.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/ErrorEvent.h"
#include "mozilla/dom/ErrorEventBinding.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/EventBinding.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/RemoteWorkerChild.h"
#include "mozilla/dom/RemoteWorkerTypes.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/ServiceWorkerManager.h"
#include "mozilla/dom/ServiceWorkerUtils.h"
#include "mozilla/dom/SimpleGlobalObject.h"
#include "mozilla/dom/Worker.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerDebuggerGlobalScopeBinding.h"
#include "mozilla/dom/WorkerGlobalScopeBinding.h"
#include "mozilla/fallible.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsGlobalWindowOuter.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"
#include "nsScriptError.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsWrapperCacheInlines.h"
#include "nscore.h"
#include "xpcpublic.h"

namespace mozilla {
namespace dom {

namespace {

class ReportErrorRunnable final : public WorkerDebuggeeRunnable {
  UniquePtr<WorkerErrorReport> mReport;

 public:
  ReportErrorRunnable(WorkerPrivate* aWorkerPrivate,
                      UniquePtr<WorkerErrorReport> aReport)
      : WorkerDebuggeeRunnable(aWorkerPrivate), mReport(std::move(aReport)) {}

 private:
  virtual void PostDispatch(WorkerPrivate* aWorkerPrivate,
                            bool aDispatchResult) override {
    aWorkerPrivate->AssertIsOnWorkerThread();

    // Dispatch may fail if the worker was canceled, no need to report that as
    // an error, so don't call base class PostDispatch.
  }

  virtual bool WorkerRun(JSContext* aCx,
                         WorkerPrivate* aWorkerPrivate) override {
    uint64_t innerWindowId;
    bool fireAtScope = true;

    bool workerIsAcceptingEvents = aWorkerPrivate->IsAcceptingEvents();

    WorkerPrivate* parent = aWorkerPrivate->GetParent();
    if (parent) {
      innerWindowId = 0;
    } else {
      AssertIsOnMainThread();

      // Once a window has frozen its workers, their
      // mMainThreadDebuggeeEventTargets should be paused, and their
      // WorkerDebuggeeRunnables should not be being executed. The same goes for
      // WorkerDebuggeeRunnables sent from child to parent workers, but since a
      // frozen parent worker runs only control runnables anyway, that is taken
      // care of naturally.
      MOZ_ASSERT(!aWorkerPrivate->IsFrozen());

      // Similarly for paused windows; all its workers should have been
      // informed. (Subworkers are unaffected by paused windows.)
      MOZ_ASSERT(!aWorkerPrivate->IsParentWindowPaused());

      if (aWorkerPrivate->IsSharedWorker()) {
        aWorkerPrivate->GetRemoteWorkerController()
            ->ErrorPropagationOnMainThread(mReport.get(),
                                           /* isErrorEvent */ true);
        return true;
      }

      // Service workers do not have a main thread parent global, so normal
      // worker error reporting will crash.  Instead, pass the error to
      // the ServiceWorkerManager to report on any controlled documents.
      if (aWorkerPrivate->IsServiceWorker()) {
        RefPtr<RemoteWorkerChild> actor(
            aWorkerPrivate->GetRemoteWorkerControllerWeakRef());

        Unused << NS_WARN_IF(!actor);

        if (actor) {
          actor->ErrorPropagationOnMainThread(nullptr, false);
        }

        return true;
      }

      // The innerWindowId is only required if we are going to ReportError
      // below, which is gated on this condition. The inner window correctness
      // check is only going to succeed when the worker is accepting events.
      if (workerIsAcceptingEvents) {
        aWorkerPrivate->AssertInnerWindowIsCorrect();
        innerWindowId = aWorkerPrivate->WindowID();
      }
    }

    // Don't fire this event if the JS object has been disconnected from the
    // private object.
    if (!workerIsAcceptingEvents) {
      return true;
    }

    WorkerErrorReport::ReportError(aCx, parent, fireAtScope,
                                   aWorkerPrivate->ParentEventTargetRef(),
                                   std::move(mReport), innerWindowId);
    return true;
  }
};

class ReportGenericErrorRunnable final : public WorkerDebuggeeRunnable {
 public:
  static void CreateAndDispatch(WorkerPrivate* aWorkerPrivate) {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();

    RefPtr<ReportGenericErrorRunnable> runnable =
        new ReportGenericErrorRunnable(aWorkerPrivate);
    runnable->Dispatch();
  }

 private:
  explicit ReportGenericErrorRunnable(WorkerPrivate* aWorkerPrivate)
      : WorkerDebuggeeRunnable(aWorkerPrivate) {
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

  void PostDispatch(WorkerPrivate* aWorkerPrivate,
                    bool aDispatchResult) override {
    aWorkerPrivate->AssertIsOnWorkerThread();

    // Dispatch may fail if the worker was canceled, no need to report that as
    // an error, so don't call base class PostDispatch.
  }

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    // Once a window has frozen its workers, their
    // mMainThreadDebuggeeEventTargets should be paused, and their
    // WorkerDebuggeeRunnables should not be being executed. The same goes for
    // WorkerDebuggeeRunnables sent from child to parent workers, but since a
    // frozen parent worker runs only control runnables anyway, that is taken
    // care of naturally.
    MOZ_ASSERT(!aWorkerPrivate->IsFrozen());

    // Similarly for paused windows; all its workers should have been informed.
    // (Subworkers are unaffected by paused windows.)
    MOZ_ASSERT(!aWorkerPrivate->IsParentWindowPaused());

    if (aWorkerPrivate->IsSharedWorker()) {
      aWorkerPrivate->GetRemoteWorkerController()->ErrorPropagationOnMainThread(
          nullptr, false);
      return true;
    }

    if (aWorkerPrivate->IsServiceWorker()) {
      RefPtr<RemoteWorkerChild> actor(
          aWorkerPrivate->GetRemoteWorkerControllerWeakRef());

      Unused << NS_WARN_IF(!actor);

      if (actor) {
        actor->ErrorPropagationOnMainThread(nullptr, false);
      }

      return true;
    }

    if (!aWorkerPrivate->IsAcceptingEvents()) {
      return true;
    }

    RefPtr<mozilla::dom::EventTarget> parentEventTarget =
        aWorkerPrivate->ParentEventTargetRef();
    RefPtr<Event> event =
        Event::Constructor(parentEventTarget, u"error"_ns, EventInit());
    event->SetTrusted(true);

    parentEventTarget->DispatchEvent(*event);
    return true;
  }
};

}  // namespace

void WorkerErrorBase::AssignErrorBase(JSErrorBase* aReport) {
  CopyUTF8toUTF16(MakeStringSpan(aReport->filename), mFilename);
  mLineNumber = aReport->lineno;
  mColumnNumber = aReport->column;
  mErrorNumber = aReport->errorNumber;
}

void WorkerErrorNote::AssignErrorNote(JSErrorNotes::Note* aNote) {
  WorkerErrorBase::AssignErrorBase(aNote);
  xpc::ErrorNote::ErrorNoteToMessageString(aNote, mMessage);
}

WorkerErrorReport::WorkerErrorReport()
    : mIsWarning(false), mExnType(JSEXN_ERR), mMutedError(false) {}

void WorkerErrorReport::AssignErrorReport(JSErrorReport* aReport) {
  WorkerErrorBase::AssignErrorBase(aReport);
  xpc::ErrorReport::ErrorReportToMessageString(aReport, mMessage);

  mLine.Assign(aReport->linebuf(), aReport->linebufLength());
  mIsWarning = aReport->isWarning();
  MOZ_ASSERT(aReport->exnType >= JSEXN_FIRST && aReport->exnType < JSEXN_LIMIT);
  mExnType = JSExnType(aReport->exnType);
  mMutedError = aReport->isMuted;

  if (aReport->notes) {
    if (!mNotes.SetLength(aReport->notes->length(), fallible)) {
      return;
    }

    size_t i = 0;
    for (auto&& note : *aReport->notes) {
      mNotes.ElementAt(i).AssignErrorNote(note.get());
      i++;
    }
  }
}

// aWorkerPrivate is the worker thread we're on (or the main thread, if null)
// aTarget is the worker object that we are going to fire an error at
// (if any).
/* static */
void WorkerErrorReport::ReportError(
    JSContext* aCx, WorkerPrivate* aWorkerPrivate, bool aFireAtScope,
    DOMEventTargetHelper* aTarget, UniquePtr<WorkerErrorReport> aReport,
    uint64_t aInnerWindowId, JS::Handle<JS::Value> aException) {
  if (aWorkerPrivate) {
    aWorkerPrivate->AssertIsOnWorkerThread();
  } else {
    AssertIsOnMainThread();
  }

  // We should not fire error events for warnings but instead make sure that
  // they show up in the error console.
  if (!aReport->mIsWarning) {
    // First fire an ErrorEvent at the worker.
    RootedDictionary<ErrorEventInit> init(aCx);

    if (aReport->mMutedError) {
      init.mMessage.AssignLiteral("Script error.");
    } else {
      init.mMessage = aReport->mMessage;
      init.mFilename = aReport->mFilename;
      init.mLineno = aReport->mLineNumber;
      init.mColno = aReport->mColumnNumber;
      init.mError = aException;
    }

    init.mCancelable = true;
    init.mBubbles = false;

    if (aTarget) {
      RefPtr<ErrorEvent> event =
          ErrorEvent::Constructor(aTarget, u"error"_ns, init);
      event->SetTrusted(true);

      bool defaultActionEnabled =
          aTarget->DispatchEvent(*event, CallerType::System, IgnoreErrors());
      if (!defaultActionEnabled) {
        return;
      }
    }

    // Now fire an event at the global object, but don't do that if the error
    // code is too much recursion and this is the same script threw the error.
    // XXXbz the interaction of this with worker errors seems kinda broken.
    // An overrecursion in the debugger or debugger sandbox will get turned
    // into an error event on our parent worker!
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1271441 tracks making this
    // better.
    if (aFireAtScope &&
        (aTarget || aReport->mErrorNumber != JSMSG_OVER_RECURSED)) {
      JS::Rooted<JSObject*> global(aCx, JS::CurrentGlobalOrNull(aCx));
      NS_ASSERTION(global, "This should never be null!");

      nsEventStatus status = nsEventStatus_eIgnore;

      if (aWorkerPrivate) {
        WorkerGlobalScope* globalScope = nullptr;
        UNWRAP_OBJECT(WorkerGlobalScope, &global, globalScope);

        if (!globalScope) {
          WorkerDebuggerGlobalScope* globalScope = nullptr;
          UNWRAP_OBJECT(WorkerDebuggerGlobalScope, &global, globalScope);

          MOZ_ASSERT_IF(globalScope,
                        globalScope->GetWrapperPreserveColor() == global);
          if (globalScope || IsWorkerDebuggerSandbox(global)) {
            aWorkerPrivate->ReportErrorToDebugger(
                aReport->mFilename, aReport->mLineNumber, aReport->mMessage);
            return;
          }

          MOZ_ASSERT(SimpleGlobalObject::SimpleGlobalType(global) ==
                     SimpleGlobalObject::GlobalType::BindingDetail);
          // XXXbz We should really log this to console, but unwinding out of
          // this stuff without ending up firing any events is ... hard.  Just
          // return for now.
          // https://bugzilla.mozilla.org/show_bug.cgi?id=1271441 tracks
          // making this better.
          return;
        }

        MOZ_ASSERT(globalScope->GetWrapperPreserveColor() == global);

        RefPtr<ErrorEvent> event =
            ErrorEvent::Constructor(aTarget, u"error"_ns, init);
        event->SetTrusted(true);

        if (NS_FAILED(EventDispatcher::DispatchDOMEvent(
                ToSupports(globalScope), nullptr, event, nullptr, &status))) {
          NS_WARNING("Failed to dispatch worker thread error event!");
          status = nsEventStatus_eIgnore;
        }
      } else if (nsGlobalWindowInner* win = xpc::WindowOrNull(global)) {
        MOZ_ASSERT(NS_IsMainThread());

        if (!win->HandleScriptError(init, &status)) {
          NS_WARNING("Failed to dispatch main thread error event!");
          status = nsEventStatus_eIgnore;
        }
      }

      // Was preventDefault() called?
      if (status == nsEventStatus_eConsumeNoDefault) {
        return;
      }
    }
  }

  // Now fire a runnable to do the same on the parent's thread if we can.
  if (aWorkerPrivate) {
    RefPtr<ReportErrorRunnable> runnable =
        new ReportErrorRunnable(aWorkerPrivate, std::move(aReport));
    runnable->Dispatch();
    return;
  }

  // Otherwise log an error to the error console.
  WorkerErrorReport::LogErrorToConsole(aCx, *aReport, aInnerWindowId);
}

/* static */
void WorkerErrorReport::LogErrorToConsole(JSContext* aCx,
                                          WorkerErrorReport& aReport,
                                          uint64_t aInnerWindowId) {
  JS::RootedObject stack(aCx, aReport.ReadStack(aCx));
  JS::RootedObject stackGlobal(aCx, JS::CurrentGlobalOrNull(aCx));

  ErrorData errorData(
      aReport.mIsWarning, aReport.mLineNumber, aReport.mColumnNumber,
      aReport.mMessage, aReport.mFilename, aReport.mLine,
      TransformIntoNewArray(aReport.mNotes, [](const WorkerErrorNote& note) {
        return ErrorDataNote(note.mLineNumber, note.mColumnNumber,
                             note.mMessage, note.mFilename);
      }));
  LogErrorToConsole(errorData, aInnerWindowId, stack, stackGlobal);
}

/* static */
void WorkerErrorReport::LogErrorToConsole(const ErrorData& aReport,
                                          uint64_t aInnerWindowId,
                                          JS::HandleObject aStack,
                                          JS::HandleObject aStackGlobal) {
  AssertIsOnMainThread();

  RefPtr<nsScriptErrorBase> scriptError =
      CreateScriptError(nullptr, JS::NothingHandleValue, aStack, aStackGlobal);

  NS_WARNING_ASSERTION(scriptError, "Failed to create script error!");

  if (scriptError) {
    nsAutoCString category("Web Worker");
    uint32_t flags = aReport.isWarning() ? nsIScriptError::warningFlag
                                         : nsIScriptError::errorFlag;
    if (NS_FAILED(scriptError->nsIScriptError::InitWithWindowID(
            aReport.message(), aReport.filename(), aReport.line(),
            aReport.lineNumber(), aReport.columnNumber(), flags, category,
            aInnerWindowId))) {
      NS_WARNING("Failed to init script error!");
      scriptError = nullptr;
    }

    for (const ErrorDataNote& note : aReport.notes()) {
      nsScriptErrorNote* noteObject = new nsScriptErrorNote();
      noteObject->Init(note.message(), note.filename(), 0, note.lineNumber(),
                       note.columnNumber());
      scriptError->AddNote(noteObject);
    }
  }

  nsCOMPtr<nsIConsoleService> consoleService =
      do_GetService(NS_CONSOLESERVICE_CONTRACTID);
  NS_WARNING_ASSERTION(consoleService, "Failed to get console service!");

  if (consoleService) {
    if (scriptError) {
      if (NS_SUCCEEDED(consoleService->LogMessage(scriptError))) {
        return;
      }
      NS_WARNING("LogMessage failed!");
    } else if (NS_SUCCEEDED(consoleService->LogStringMessage(
                   aReport.message().BeginReading()))) {
      return;
    }
    NS_WARNING("LogStringMessage failed!");
  }

  NS_ConvertUTF16toUTF8 msg(aReport.message());
  NS_ConvertUTF16toUTF8 filename(aReport.filename());

  static const char kErrorString[] = "JS error in Web Worker: %s [%s:%u]";

#ifdef ANDROID
  __android_log_print(ANDROID_LOG_INFO, "Gecko", kErrorString, msg.get(),
                      filename.get(), aReport.lineNumber());
#endif

  fprintf(stderr, kErrorString, msg.get(), filename.get(),
          aReport.lineNumber());
  fflush(stderr);
}

/* static */
void WorkerErrorReport::CreateAndDispatchGenericErrorRunnableToParent(
    WorkerPrivate* aWorkerPrivate) {
  ReportGenericErrorRunnable::CreateAndDispatch(aWorkerPrivate);
}

}  // namespace dom
}  // namespace mozilla
