/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Console_h
#define mozilla_dom_Console_h

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/JSObjectHolder.h"
#include "mozilla/TimeStamp.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "nsDOMNavigationTiming.h"
#include "nsPIDOMWindow.h"

class nsIConsoleAPIStorage;
class nsIPrincipal;
class nsIStackFrame;

namespace mozilla {
namespace dom {

class AnyCallback;
class ConsoleCallData;
class ConsoleInstance;
class ConsoleInstanceDumpCallback;
class ConsoleRunnable;
class ConsoleCallDataRunnable;
class ConsoleProfileRunnable;
struct ConsoleInstanceOptions;
struct ConsoleTimerError;
struct ConsoleStackEntry;

class Console final : public nsIObserver
                    , public nsSupportsWeakReference
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(Console, nsIObserver)
  NS_DECL_NSIOBSERVER

  static already_AddRefed<Console>
  Create(nsPIDOMWindowInner* aWindow, ErrorResult& aRv);

  // WebIDL methods
  nsPIDOMWindowInner* GetParentObject() const
  {
    return mWindow;
  }

  static void
  Log(const GlobalObject& aGlobal, const Sequence<JS::Value>& aData);

  static void
  Info(const GlobalObject& aGlobal, const Sequence<JS::Value>& aData);

  static void
  Warn(const GlobalObject& aGlobal, const Sequence<JS::Value>& aData);

  static void
  Error(const GlobalObject& aGlobal, const Sequence<JS::Value>& aData);

  static void
  Exception(const GlobalObject& aGlobal, const Sequence<JS::Value>& aData);

  static void
  Debug(const GlobalObject& aGlobal, const Sequence<JS::Value>& aData);

  static void
  Table(const GlobalObject& aGlobal, const Sequence<JS::Value>& aData);

  static void
  Trace(const GlobalObject& aGlobal, const Sequence<JS::Value>& aData);

  static void
  Dir(const GlobalObject& aGlobal, const Sequence<JS::Value>& aData);

  static void
  Dirxml(const GlobalObject& aGlobal, const Sequence<JS::Value>& aData);

  static void
  Group(const GlobalObject& aGlobal, const Sequence<JS::Value>& aData);

  static void
  GroupCollapsed(const GlobalObject& aGlobal, const Sequence<JS::Value>& aData);

  static void
  GroupEnd(const GlobalObject& aGlobal);

  static void
  Time(const GlobalObject& aGlobal, const nsAString& aLabel);

  static void
  TimeEnd(const GlobalObject& aGlobal, const nsAString& aLabel);

  static void
  TimeStamp(const GlobalObject& aGlobal, const JS::Handle<JS::Value> aData);

  static void
  Profile(const GlobalObject& aGlobal, const Sequence<JS::Value>& aData);

  static void
  ProfileEnd(const GlobalObject& aGlobal, const Sequence<JS::Value>& aData);

  static void
  Assert(const GlobalObject& aGlobal, bool aCondition,
         const Sequence<JS::Value>& aData);

  static void
  Count(const GlobalObject& aGlobal, const nsAString& aLabel);

  static void
  Clear(const GlobalObject& aGlobal);

  static already_AddRefed<ConsoleInstance>
  CreateInstance(const GlobalObject& aGlobal,
                 const ConsoleInstanceOptions& aOptions);

  void
  ClearStorage();

  void
  RetrieveConsoleEvents(JSContext* aCx, nsTArray<JS::Value>& aEvents,
                        ErrorResult& aRv);

  void
  SetConsoleEventHandler(AnyCallback* aHandler);

private:
  explicit Console(nsPIDOMWindowInner* aWindow);
  ~Console();

  void
  Initialize(ErrorResult& aRv);

  void
  Shutdown();

  enum MethodName
  {
    MethodLog,
    MethodInfo,
    MethodWarn,
    MethodError,
    MethodException,
    MethodDebug,
    MethodTable,
    MethodTrace,
    MethodDir,
    MethodDirxml,
    MethodGroup,
    MethodGroupCollapsed,
    MethodGroupEnd,
    MethodTime,
    MethodTimeEnd,
    MethodTimeStamp,
    MethodAssert,
    MethodCount,
    MethodClear
  };

  static already_AddRefed<Console>
  GetConsole(const GlobalObject& aGlobal);

  static already_AddRefed<Console>
  GetConsoleInternal(const GlobalObject& aGlobal, ErrorResult &aRv);

  static void
  ProfileMethod(const GlobalObject& aGlobal, const nsAString& aAction,
                const Sequence<JS::Value>& aData);

  void
  ProfileMethodInternal(JSContext* aCx, const nsAString& aAction,
                        const Sequence<JS::Value>& aData);

  static void
  Method(const GlobalObject& aGlobal, MethodName aName,
         const nsAString& aString, const Sequence<JS::Value>& aData);

  void
  MethodInternal(JSContext* aCx, MethodName aName,
                 const nsAString& aString, const Sequence<JS::Value>& aData);

  static void
  StringMethod(const GlobalObject& aGlobal, const nsAString& aLabel,
               MethodName aMethodName, const nsAString& aMethodString);

  void
  StringMethodInternal(JSContext* aCx, const nsAString& aLabel,
                       MethodName aMethodName, const nsAString& aMethodString);

  // This method must receive aCx and aArguments in the same JSCompartment.
  void
  ProcessCallData(JSContext* aCx,
                  ConsoleCallData* aData,
                  const Sequence<JS::Value>& aArguments);

  void
  StoreCallData(ConsoleCallData* aData);

  void
  UnstoreCallData(ConsoleCallData* aData);

  // Read in Console.cpp how this method is used.
  void
  ReleaseCallData(ConsoleCallData* aCallData);

  // aCx and aArguments must be in the same JS compartment.
  void
  NotifyHandler(JSContext* aCx,
                const Sequence<JS::Value>& aArguments,
                ConsoleCallData* aData);

  // PopulateConsoleNotificationInTheTargetScope receives aCx and aArguments in
  // the same JS compartment and populates the ConsoleEvent object (aValue) in
  // the aTargetScope.
  // aTargetScope can be:
  // - the system-principal scope when we want to dispatch the ConsoleEvent to
  //   nsIConsoleAPIStorage (See the comment in Console.cpp about the use of
  //   xpc::PrivilegedJunkScope()
  // - the mConsoleEventNotifier->Callable() scope when we want to notify this
  //   handler about a new ConsoleEvent.
  // - It can be the global from the JSContext when RetrieveConsoleEvents is
  //   called.
  bool
  PopulateConsoleNotificationInTheTargetScope(JSContext* aCx,
                                              const Sequence<JS::Value>& aArguments,
                                              JSObject* aTargetScope,
                                              JS::MutableHandle<JS::Value> aValue,
                                              ConsoleCallData* aData);

  // If the first JS::Value of the array is a string, this method uses it to
  // format a string. The supported sequences are:
  //   %s    - string
  //   %d,%i - integer
  //   %f    - double
  //   %o,%O - a JS object.
  //   %c    - style string.
  // The output is an array where any object is a separated item, the rest is
  // unified in a format string.
  // Example if the input is:
  //   "string: %s, integer: %d, object: %o, double: %d", 's', 1, window, 0.9
  // The output will be:
  //   [ "string: s, integer: 1, object: ", window, ", double: 0.9" ]
  //
  // The aStyles array is populated with the style strings that the function
  // finds based the format string. The index of the styles matches the indexes
  // of elements that need the custom styling from aSequence. For elements with
  // no custom styling the array is padded with null elements.
  bool
  ProcessArguments(JSContext* aCx, const Sequence<JS::Value>& aData,
                   Sequence<JS::Value>& aSequence,
                   Sequence<nsString>& aStyles) const;

  void
  MakeFormatString(nsCString& aFormat, int32_t aInteger, int32_t aMantissa,
                   char aCh) const;

  // Stringify and Concat all the JS::Value in a single string using ' ' as
  // separator. The new group name will be stored in mGroupStack array.
  void
  ComposeAndStoreGroupName(JSContext* aCx, const Sequence<JS::Value>& aData,
                           nsAString& aName);

  // Remove the last group name and return that name. It returns false if
  // mGroupStack is empty.
  bool
  UnstoreGroupName(nsAString& aName);

  enum TimerStatus {
    eTimerUnknown,
    eTimerDone,
    eTimerAlreadyExists,
    eTimerDoesntExist,
    eTimerJSException,
    eTimerMaxReached,
  };

  JS::Value
  CreateTimerError(JSContext* aCx, const nsAString& aTimerLabel,
                   TimerStatus aStatus) const;

  // StartTimer is called on the owning thread and populates aTimerLabel and
  // aTimerValue.
  // * aCx - the JSContext rooting aName.
  // * aName - this is (should be) the name of the timer as JS::Value.
  // * aTimestamp - the monotonicTimer for this context taken from
  //                performance.now().
  // * aTimerLabel - This label will be populated with the aName converted to a
  //                 string.
  // * aTimerValue - the StartTimer value stored into (or taken from)
  //                 mTimerRegistry.
  TimerStatus
  StartTimer(JSContext* aCx, const JS::Value& aName,
             DOMHighResTimeStamp aTimestamp,
             nsAString& aTimerLabel,
             DOMHighResTimeStamp* aTimerValue);

  // CreateStartTimerValue generates a ConsoleTimerStart dictionary exposed as
  // JS::Value. If aTimerStatus is false, it generates a ConsoleTimerError
  // instead. It's called only after the execution StartTimer on the owning
  // thread.
  // * aCx - this is the context that will root the returned value.
  // * aTimerLabel - this label must be what StartTimer received as aTimerLabel.
  // * aTimerStatus - the return value of StartTimer.
  JS::Value
  CreateStartTimerValue(JSContext* aCx, const nsAString& aTimerLabel,
                        TimerStatus aTimerStatus) const;

  // StopTimer follows the same pattern as StartTimer: it runs on the
  // owning thread and populates aTimerLabel and aTimerDuration, used by
  // CreateStopTimerValue.
  // * aCx - the JSContext rooting aName.
  // * aName - this is (should be) the name of the timer as JS::Value.
  // * aTimestamp - the monotonicTimer for this context taken from
  //                performance.now().
  // * aTimerLabel - This label will be populated with the aName converted to a
  //                 string.
  // * aTimerDuration - the difference between aTimestamp and when the timer
  //                    started (see StartTimer).
  TimerStatus
  StopTimer(JSContext* aCx, const JS::Value& aName,
            DOMHighResTimeStamp aTimestamp,
            nsAString& aTimerLabel,
            double* aTimerDuration);

  // This method generates a ConsoleTimerEnd dictionary exposed as JS::Value, or
  // a ConsoleTimerError dictionary if aTimerStatus is false. See StopTimer.
  // * aCx - this is the context that will root the returned value.
  // * aTimerLabel - this label must be what StopTimer received as aTimerLabel.
  // * aTimerDuration - this is what StopTimer received as aTimerDuration
  // * aTimerStatus - the return value of StopTimer.
  JS::Value
  CreateStopTimerValue(JSContext* aCx, const nsAString& aTimerLabel,
                       double aTimerDuration,
                       TimerStatus aTimerStatus) const;

  // The method populates a Sequence from an array of JS::Value.
  bool
  ArgumentsToValueList(const Sequence<JS::Value>& aData,
                       Sequence<JS::Value>& aSequence) const;

  // This method follows the same pattern as StartTimer: its runs on the owning
  // thread and populate aCountLabel, used by CreateCounterValue. Returns
  // 3 possible values:
  // * MAX_PAGE_COUNTERS in case of error that has to be reported;
  // * 0 in case of a CX exception. The operation cannot continue;
  // * the incremented counter value.
  // Params:
  // * aCx - the JSContext rooting aData.
  // * aData - the arguments received by the console.count() method.
  // * aCountLabel - the label that will be populated by this method.
  uint32_t
  IncreaseCounter(JSContext* aCx, const Sequence<JS::Value>& aData,
                  nsAString& aCountLabel);

  // This method generates a ConsoleCounter dictionary as JS::Value. If
  // aCountValue is == MAX_PAGE_COUNTERS it generates a ConsoleCounterError
  // instead. See IncreaseCounter.
  // * aCx - this is the context that will root the returned value.
  // * aCountLabel - this label must be what IncreaseCounter received as
  //                 aTimerLabel.
  // * aCountValue - the return value of IncreaseCounter.
  JS::Value
  CreateCounterValue(JSContext* aCx, const nsAString& aCountLabel,
                     uint32_t aCountValue) const;

  bool
  ShouldIncludeStackTrace(MethodName aMethodName) const;

  JSObject*
  GetOrCreateSandbox(JSContext* aCx, nsIPrincipal* aPrincipal);

  void
  AssertIsOnOwningThread() const;

  bool
  IsShuttingDown() const;

  bool
  MonotonicTimer(JSContext* aCx, MethodName aMethodName,
                 const Sequence<JS::Value>& aData,
                 DOMHighResTimeStamp* aTimeStamp);

  void
  MaybeExecuteDumpFunction(JSContext* aCx, const nsAString& aMethodName,
                           const Sequence<JS::Value>& aData);

  void
  MaybeExecuteDumpFunctionForTrace(JSContext* aCx, nsIStackFrame* aStack);

  void
  ExecuteDumpFunction(const nsAString& aMessage);

  // All these nsCOMPtr are touched on main thread only.
  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  nsCOMPtr<nsIConsoleAPIStorage> mStorage;
  RefPtr<JSObjectHolder> mSandbox;

  // Touched on the owner thread.
  nsDataHashtable<nsStringHashKey, DOMHighResTimeStamp> mTimerRegistry;
  nsDataHashtable<nsStringHashKey, uint32_t> mCounterRegistry;

  nsTArray<RefPtr<ConsoleCallData>> mCallDataStorage;

  // This array is used in a particular corner-case where:
  // 1. we are in a worker thread
  // 2. we have more than STORAGE_MAX_EVENTS
  // 3. but the main-thread ConsoleCallDataRunnable of the first one is still
  // running (this means that something very bad is happening on the
  // main-thread).
  // When this happens we want to keep the ConsoleCallData alive for traceing
  // its JSValues also if 'officially' this ConsoleCallData must be removed from
  // the storage.
  nsTArray<RefPtr<ConsoleCallData>> mCallDataStoragePending;

  RefPtr<AnyCallback> mConsoleEventNotifier;

  // This is the stack for groupping.
  nsTArray<nsString> mGroupStack;

  uint64_t mOuterID;
  uint64_t mInnerID;

  // Set only by ConsoleInstance:
  nsString mConsoleID;
  nsString mPassedInnerID;
  RefPtr<ConsoleInstanceDumpCallback> mDumpFunction;
  bool mDumpToStdout;

  enum {
    eUnknown,
    eInitialized,
    eShuttingDown
  } mStatus;

  // This is used when Console is created and it's used only for JSM custom
  // console instance.
  mozilla::TimeStamp mCreationTimeStamp;

  friend class ConsoleCallData;
  friend class ConsoleInstance;
  friend class ConsoleRunnable;
  friend class ConsoleCallDataRunnable;
  friend class ConsoleProfileRunnable;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_Console_h */
