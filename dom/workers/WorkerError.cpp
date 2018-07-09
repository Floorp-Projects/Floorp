/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerError.h"

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/ErrorEvent.h"
#include "mozilla/dom/ErrorEventBinding.h"
#include "mozilla/dom/ServiceWorkerManager.h"
#include "mozilla/dom/SimpleGlobalObject.h"
#include "mozilla/dom/WorkerDebuggerGlobalScopeBinding.h"
#include "mozilla/dom/WorkerGlobalScopeBinding.h"
#include "mozilla/EventDispatcher.h"
#include "nsGlobalWindowInner.h"
#include "nsIConsoleService.h"
#include "nsScriptError.h"
#include "WorkerRunnable.h"
#include "WorkerPrivate.h"
#include "WorkerScope.h"

namespace mozilla {
namespace dom {

namespace {

class ReportErrorRunnable final : public WorkerRunnable
{
  WorkerErrorReport mReport;

public:
  // aWorkerPrivate is the worker thread we're on (or the main thread, if null)
  // aTarget is the worker object that we are going to fire an error at
  // (if any).
  static void
  ReportError(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
              bool aFireAtScope, DOMEventTargetHelper* aTarget,
              const WorkerErrorReport& aReport, uint64_t aInnerWindowId,
              JS::Handle<JS::Value> aException = JS::NullHandleValue)
  {
    if (aWorkerPrivate) {
      aWorkerPrivate->AssertIsOnWorkerThread();
    } else {
      AssertIsOnMainThread();
    }

    // We should not fire error events for warnings but instead make sure that
    // they show up in the error console.
    if (!JSREPORT_IS_WARNING(aReport.mFlags)) {
      // First fire an ErrorEvent at the worker.
      RootedDictionary<ErrorEventInit> init(aCx);

      if (aReport.mMutedError) {
        init.mMessage.AssignLiteral("Script error.");
      } else {
        init.mMessage = aReport.mMessage;
        init.mFilename = aReport.mFilename;
        init.mLineno = aReport.mLineNumber;
        init.mError = aException;
      }

      init.mCancelable = true;
      init.mBubbles = false;

      if (aTarget) {
        RefPtr<ErrorEvent> event =
          ErrorEvent::Constructor(aTarget, NS_LITERAL_STRING("error"), init);
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
          (aTarget || aReport.mErrorNumber != JSMSG_OVER_RECURSED)) {
        JS::Rooted<JSObject*> global(aCx, JS::CurrentGlobalOrNull(aCx));
        NS_ASSERTION(global, "This should never be null!");

        nsEventStatus status = nsEventStatus_eIgnore;

        if (aWorkerPrivate) {
          WorkerGlobalScope* globalScope = nullptr;
          UNWRAP_OBJECT(WorkerGlobalScope, &global, globalScope);

          if (!globalScope) {
            WorkerDebuggerGlobalScope* globalScope = nullptr;
            UNWRAP_OBJECT(WorkerDebuggerGlobalScope, &global, globalScope);

            MOZ_ASSERT_IF(globalScope, globalScope->GetWrapperPreserveColor() == global);
            if (globalScope || IsWorkerDebuggerSandbox(global)) {
              aWorkerPrivate->ReportErrorToDebugger(aReport.mFilename, aReport.mLineNumber,
                                                    aReport.mMessage);
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
            ErrorEvent::Constructor(aTarget, NS_LITERAL_STRING("error"), init);
          event->SetTrusted(true);

          if (NS_FAILED(EventDispatcher::DispatchDOMEvent(ToSupports(globalScope),
                                                          nullptr,
                                                          event, nullptr,
                                                          &status))) {
            NS_WARNING("Failed to dispatch worker thread error event!");
            status = nsEventStatus_eIgnore;
          }
        }
        else if (nsGlobalWindowInner* win = xpc::WindowOrNull(global)) {
          MOZ_ASSERT(NS_IsMainThread());

          if (NS_FAILED(win->HandleScriptError(init, &status))) {
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
        new ReportErrorRunnable(aWorkerPrivate, aReport);
      runnable->Dispatch();
      return;
    }

    // Otherwise log an error to the error console.
    WorkerErrorReport::LogErrorToConsole(aReport, aInnerWindowId);
  }

  ReportErrorRunnable(WorkerPrivate* aWorkerPrivate,
                      const WorkerErrorReport& aReport)
  : WorkerRunnable(aWorkerPrivate, ParentThreadUnchangedBusyCount),
    mReport(aReport)
  { }

private:
  virtual void
  PostDispatch(WorkerPrivate* aWorkerPrivate, bool aDispatchResult) override
  {
    aWorkerPrivate->AssertIsOnWorkerThread();

    // Dispatch may fail if the worker was canceled, no need to report that as
    // an error, so don't call base class PostDispatch.
  }

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    uint64_t innerWindowId;
    bool fireAtScope = true;

    bool workerIsAcceptingEvents = aWorkerPrivate->IsAcceptingEvents();

    WorkerPrivate* parent = aWorkerPrivate->GetParent();
    if (parent) {
      innerWindowId = 0;
    }
    else {
      AssertIsOnMainThread();

      if (aWorkerPrivate->IsFrozen() ||
          aWorkerPrivate->IsParentWindowPaused()) {
        MOZ_ASSERT(!IsDebuggerRunnable());
        aWorkerPrivate->QueueRunnable(this);
        return true;
      }

      if (aWorkerPrivate->IsSharedWorker()) {
        aWorkerPrivate->BroadcastErrorToSharedWorkers(aCx, &mReport,
                                                      /* isErrorEvent */ true);
        return true;
      }

      // Service workers do not have a main thread parent global, so normal
      // worker error reporting will crash.  Instead, pass the error to
      // the ServiceWorkerManager to report on any controlled documents.
      if (aWorkerPrivate->IsServiceWorker()) {
        RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
        if (swm) {
          swm->HandleError(aCx, aWorkerPrivate->GetPrincipal(),
                           aWorkerPrivate->ServiceWorkerScope(),
                           aWorkerPrivate->ScriptURL(),
                           mReport.mMessage,
                           mReport.mFilename, mReport.mLine, mReport.mLineNumber,
                           mReport.mColumnNumber, mReport.mFlags,
                           mReport.mExnType);
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

    ReportError(aCx, parent, fireAtScope,
                aWorkerPrivate->ParentEventTargetRef(),
                mReport, innerWindowId);
    return true;
  }
};

} // anonymous

void
WorkerErrorBase::AssignErrorBase(JSErrorBase* aReport)
{
  mFilename = NS_ConvertUTF8toUTF16(aReport->filename);
  mLineNumber = aReport->lineno;
  mColumnNumber = aReport->column;
  mErrorNumber = aReport->errorNumber;
}

void
WorkerErrorNote::AssignErrorNote(JSErrorNotes::Note* aNote)
{
  WorkerErrorBase::AssignErrorBase(aNote);
  xpc::ErrorNote::ErrorNoteToMessageString(aNote, mMessage);
}

void
WorkerErrorReport::AssignErrorReport(JSErrorReport* aReport)
{
  WorkerErrorBase::AssignErrorBase(aReport);
  xpc::ErrorReport::ErrorReportToMessageString(aReport, mMessage);

  mLine.Assign(aReport->linebuf(), aReport->linebufLength());
  mFlags = aReport->flags;
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
/* static */ void
WorkerErrorReport::ReportError(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                               bool aFireAtScope, DOMEventTargetHelper* aTarget,
                               const WorkerErrorReport& aReport,
                               uint64_t aInnerWindowId,
                               JS::Handle<JS::Value> aException)
{
  if (aWorkerPrivate) {
    aWorkerPrivate->AssertIsOnWorkerThread();
  } else {
    AssertIsOnMainThread();
  }

  // We should not fire error events for warnings but instead make sure that
  // they show up in the error console.
  if (!JSREPORT_IS_WARNING(aReport.mFlags)) {
    // First fire an ErrorEvent at the worker.
    RootedDictionary<ErrorEventInit> init(aCx);

    if (aReport.mMutedError) {
      init.mMessage.AssignLiteral("Script error.");
    } else {
      init.mMessage = aReport.mMessage;
      init.mFilename = aReport.mFilename;
      init.mLineno = aReport.mLineNumber;
      init.mError = aException;
    }

    init.mCancelable = true;
    init.mBubbles = false;

    if (aTarget) {
      RefPtr<ErrorEvent> event =
        ErrorEvent::Constructor(aTarget, NS_LITERAL_STRING("error"), init);
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
        (aTarget || aReport.mErrorNumber != JSMSG_OVER_RECURSED)) {
      JS::Rooted<JSObject*> global(aCx, JS::CurrentGlobalOrNull(aCx));
      NS_ASSERTION(global, "This should never be null!");

      nsEventStatus status = nsEventStatus_eIgnore;

      if (aWorkerPrivate) {
        WorkerGlobalScope* globalScope = nullptr;
        UNWRAP_OBJECT(WorkerGlobalScope, &global, globalScope);

        if (!globalScope) {
          WorkerDebuggerGlobalScope* globalScope = nullptr;
          UNWRAP_OBJECT(WorkerDebuggerGlobalScope, &global, globalScope);

          MOZ_ASSERT_IF(globalScope, globalScope->GetWrapperPreserveColor() == global);
          if (globalScope || IsWorkerDebuggerSandbox(global)) {
            aWorkerPrivate->ReportErrorToDebugger(aReport.mFilename, aReport.mLineNumber,
                                                  aReport.mMessage);
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
          ErrorEvent::Constructor(aTarget, NS_LITERAL_STRING("error"), init);
        event->SetTrusted(true);

        if (NS_FAILED(EventDispatcher::DispatchDOMEvent(ToSupports(globalScope),
                                                        nullptr,
                                                        event, nullptr,
                                                        &status))) {
          NS_WARNING("Failed to dispatch worker thread error event!");
          status = nsEventStatus_eIgnore;
        }
      }
      else if (nsGlobalWindowInner* win = xpc::WindowOrNull(global)) {
        MOZ_ASSERT(NS_IsMainThread());

        if (NS_FAILED(win->HandleScriptError(init, &status))) {
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
      new ReportErrorRunnable(aWorkerPrivate, aReport);
    runnable->Dispatch();
    return;
  }

  // Otherwise log an error to the error console.
  WorkerErrorReport::LogErrorToConsole(aReport, aInnerWindowId);
}

/* static */ void
WorkerErrorReport::LogErrorToConsole(const WorkerErrorReport& aReport,
                                     uint64_t aInnerWindowId)
{
  AssertIsOnMainThread();

  RefPtr<nsScriptErrorBase> scriptError = new nsScriptError();
  NS_WARNING_ASSERTION(scriptError, "Failed to create script error!");

  if (scriptError) {
    nsAutoCString category("Web Worker");
    if (NS_FAILED(scriptError->InitWithWindowID(aReport.mMessage,
                                                aReport.mFilename,
                                                aReport.mLine,
                                                aReport.mLineNumber,
                                                aReport.mColumnNumber,
                                                aReport.mFlags,
                                                category,
                                                aInnerWindowId))) {
      NS_WARNING("Failed to init script error!");
      scriptError = nullptr;
    }

    for (size_t i = 0, len = aReport.mNotes.Length(); i < len; i++) {
      const WorkerErrorNote& note = aReport.mNotes.ElementAt(i);

      nsScriptErrorNote* noteObject = new nsScriptErrorNote();
      noteObject->Init(note.mMessage, note.mFilename,
                       note.mLineNumber, note.mColumnNumber);
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
                              aReport.mMessage.BeginReading()))) {
      return;
    }
    NS_WARNING("LogStringMessage failed!");
  }

  NS_ConvertUTF16toUTF8 msg(aReport.mMessage);
  NS_ConvertUTF16toUTF8 filename(aReport.mFilename);

  static const char kErrorString[] = "JS error in Web Worker: %s [%s:%u]";

#ifdef ANDROID
  __android_log_print(ANDROID_LOG_INFO, "Gecko", kErrorString, msg.get(),
                      filename.get(), aReport.mLineNumber);
#endif

  fprintf(stderr, kErrorString, msg.get(), filename.get(), aReport.mLineNumber);
  fflush(stderr);
}

} // dom namespace
} // mozilla namespace
