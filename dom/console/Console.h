/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Console_h
#define mozilla_dom_Console_h

#include "mozilla/dom/ConsoleBinding.h"
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

class Console final : public nsIObserver, public nsSupportsWeakReference {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(Console, nsIObserver)
  NS_DECL_NSIOBSERVER

  static already_AddRefed<Console> Create(JSContext* aCx,
                                          nsPIDOMWindowInner* aWindow,
                                          ErrorResult& aRv);

  static already_AddRefed<Console> CreateForWorklet(JSContext* aCx,
                                                    nsIGlobalObject* aGlobal,
                                                    uint64_t aOuterWindowID,
                                                    uint64_t aInnerWindowID,
                                                    ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT
  static void Log(const GlobalObject& aGlobal,
                  const Sequence<JS::Value>& aData);

  MOZ_CAN_RUN_SCRIPT
  static void Info(const GlobalObject& aGlobal,
                   const Sequence<JS::Value>& aData);

  MOZ_CAN_RUN_SCRIPT
  static void Warn(const GlobalObject& aGlobal,
                   const Sequence<JS::Value>& aData);

  MOZ_CAN_RUN_SCRIPT
  static void Error(const GlobalObject& aGlobal,
                    const Sequence<JS::Value>& aData);

  MOZ_CAN_RUN_SCRIPT
  static void Exception(const GlobalObject& aGlobal,
                        const Sequence<JS::Value>& aData);

  MOZ_CAN_RUN_SCRIPT
  static void Debug(const GlobalObject& aGlobal,
                    const Sequence<JS::Value>& aData);

  MOZ_CAN_RUN_SCRIPT
  static void Table(const GlobalObject& aGlobal,
                    const Sequence<JS::Value>& aData);

  MOZ_CAN_RUN_SCRIPT
  static void Trace(const GlobalObject& aGlobal,
                    const Sequence<JS::Value>& aData);

  MOZ_CAN_RUN_SCRIPT
  static void Dir(const GlobalObject& aGlobal,
                  const Sequence<JS::Value>& aData);

  MOZ_CAN_RUN_SCRIPT
  static void Dirxml(const GlobalObject& aGlobal,
                     const Sequence<JS::Value>& aData);

  MOZ_CAN_RUN_SCRIPT
  static void Group(const GlobalObject& aGlobal,
                    const Sequence<JS::Value>& aData);

  MOZ_CAN_RUN_SCRIPT
  static void GroupCollapsed(const GlobalObject& aGlobal,
                             const Sequence<JS::Value>& aData);

  MOZ_CAN_RUN_SCRIPT
  static void GroupEnd(const GlobalObject& aGlobal);

  MOZ_CAN_RUN_SCRIPT
  static void Time(const GlobalObject& aGlobal, const nsAString& aLabel);

  MOZ_CAN_RUN_SCRIPT
  static void TimeLog(const GlobalObject& aGlobal, const nsAString& aLabel,
                      const Sequence<JS::Value>& aData);

  MOZ_CAN_RUN_SCRIPT
  static void TimeEnd(const GlobalObject& aGlobal, const nsAString& aLabel);

  MOZ_CAN_RUN_SCRIPT
  static void TimeStamp(const GlobalObject& aGlobal,
                        const JS::Handle<JS::Value> aData);

  MOZ_CAN_RUN_SCRIPT
  static void Profile(const GlobalObject& aGlobal,
                      const Sequence<JS::Value>& aData);

  MOZ_CAN_RUN_SCRIPT
  static void ProfileEnd(const GlobalObject& aGlobal,
                         const Sequence<JS::Value>& aData);

  MOZ_CAN_RUN_SCRIPT
  static void Assert(const GlobalObject& aGlobal, bool aCondition,
                     const Sequence<JS::Value>& aData);

  MOZ_CAN_RUN_SCRIPT
  static void Count(const GlobalObject& aGlobal, const nsAString& aLabel);

  MOZ_CAN_RUN_SCRIPT
  static void CountReset(const GlobalObject& aGlobal, const nsAString& aLabel);

  MOZ_CAN_RUN_SCRIPT
  static void Clear(const GlobalObject& aGlobal);

  static already_AddRefed<ConsoleInstance> CreateInstance(
      const GlobalObject& aGlobal, const ConsoleInstanceOptions& aOptions);

  void ClearStorage();

  void RetrieveConsoleEvents(JSContext* aCx, nsTArray<JS::Value>& aEvents,
                             ErrorResult& aRv);

  void SetConsoleEventHandler(AnyCallback* aHandler);

 private:
  Console(JSContext* aCx, nsIGlobalObject* aGlobal, uint64_t aOuterWindowID,
          uint64_t aInnerWIndowID);
  ~Console();

  void Initialize(ErrorResult& aRv);

  void Shutdown();

  enum MethodName {
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
    MethodTimeLog,
    MethodTimeEnd,
    MethodTimeStamp,
    MethodAssert,
    MethodCount,
    MethodCountReset,
    MethodClear,
    MethodProfile,
    MethodProfileEnd,
  };

  static already_AddRefed<Console> GetConsole(const GlobalObject& aGlobal);

  static already_AddRefed<Console> GetConsoleInternal(
      const GlobalObject& aGlobal, ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT
  static void ProfileMethod(const GlobalObject& aGlobal, MethodName aName,
                            const nsAString& aAction,
                            const Sequence<JS::Value>& aData);

  MOZ_CAN_RUN_SCRIPT
  void ProfileMethodInternal(JSContext* aCx, MethodName aName,
                             const nsAString& aAction,
                             const Sequence<JS::Value>& aData);

  // Implementation of the mainthread-only parts of ProfileMethod.
  // This is indepedent of console instance state.
  static void ProfileMethodMainthread(JSContext* aCx, const nsAString& aAction,
                                      const Sequence<JS::Value>& aData);

  MOZ_CAN_RUN_SCRIPT
  static void Method(const GlobalObject& aGlobal, MethodName aName,
                     const nsAString& aString,
                     const Sequence<JS::Value>& aData);

  MOZ_CAN_RUN_SCRIPT
  void MethodInternal(JSContext* aCx, MethodName aName,
                      const nsAString& aString,
                      const Sequence<JS::Value>& aData);

  MOZ_CAN_RUN_SCRIPT
  static void StringMethod(const GlobalObject& aGlobal, const nsAString& aLabel,
                           const Sequence<JS::Value>& aData,
                           MethodName aMethodName,
                           const nsAString& aMethodString);

  MOZ_CAN_RUN_SCRIPT
  void StringMethodInternal(JSContext* aCx, const nsAString& aLabel,
                            const Sequence<JS::Value>& aData,
                            MethodName aMethodName,
                            const nsAString& aMethodString);

  // This method must receive aCx and aArguments in the same JS::Compartment.
  void ProcessCallData(JSContext* aCx, ConsoleCallData* aData,
                       const Sequence<JS::Value>& aArguments);

  // Returns true on success; otherwise false.
  bool StoreCallData(JSContext* aCx, ConsoleCallData* aCallData,
                     const Sequence<JS::Value>& aArguments);

  void UnstoreCallData(ConsoleCallData* aData);

  // aCx and aArguments must be in the same JS compartment.
  MOZ_CAN_RUN_SCRIPT
  void NotifyHandler(JSContext* aCx, const Sequence<JS::Value>& aArguments,
                     ConsoleCallData* aData);

  // PopulateConsoleNotificationInTheTargetScope receives aCx and aArguments in
  // the same JS compartment and populates the ConsoleEvent object (aValue) in
  // the aTargetScope.
  // aTargetScope can be:
  // - the system-principal scope when we want to dispatch the ConsoleEvent to
  //   nsIConsoleAPIStorage (See the comment in Console.cpp about the use of
  //   xpc::PrivilegedJunkScope()
  // - the mConsoleEventNotifier->CallableGlobal() when we want to notify this
  //   handler about a new ConsoleEvent.
  // - It can be the global from the JSContext when RetrieveConsoleEvents is
  //   called.
  bool PopulateConsoleNotificationInTheTargetScope(
      JSContext* aCx, const Sequence<JS::Value>& aArguments,
      JS::Handle<JSObject*> aTargetScope, JS::MutableHandle<JS::Value> aValue,
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
  bool ProcessArguments(JSContext* aCx, const Sequence<JS::Value>& aData,
                        Sequence<JS::Value>& aSequence,
                        Sequence<nsString>& aStyles) const;

  void MakeFormatString(nsCString& aFormat, int32_t aInteger, int32_t aMantissa,
                        char aCh) const;

  // Stringify and Concat all the JS::Value in a single string using ' ' as
  // separator. The new group name will be stored in mGroupStack array.
  void ComposeAndStoreGroupName(JSContext* aCx,
                                const Sequence<JS::Value>& aData,
                                nsAString& aName);

  // Remove the last group name and return that name. It returns false if
  // mGroupStack is empty.
  bool UnstoreGroupName(nsAString& aName);

  enum TimerStatus {
    eTimerUnknown,
    eTimerDone,
    eTimerAlreadyExists,
    eTimerDoesntExist,
    eTimerJSException,
    eTimerMaxReached,
  };

  JS::Value CreateTimerError(JSContext* aCx, const nsAString& aTimerLabel,
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
  TimerStatus StartTimer(JSContext* aCx, const JS::Value& aName,
                         DOMHighResTimeStamp aTimestamp, nsAString& aTimerLabel,
                         DOMHighResTimeStamp* aTimerValue);

  // CreateStartTimerValue generates a ConsoleTimerStart dictionary exposed as
  // JS::Value. If aTimerStatus is false, it generates a ConsoleTimerError
  // instead. It's called only after the execution StartTimer on the owning
  // thread.
  // * aCx - this is the context that will root the returned value.
  // * aTimerLabel - this label must be what StartTimer received as aTimerLabel.
  // * aTimerStatus - the return value of StartTimer.
  JS::Value CreateStartTimerValue(JSContext* aCx, const nsAString& aTimerLabel,
                                  TimerStatus aTimerStatus) const;

  // LogTimer follows the same pattern as StartTimer: it runs on the
  // owning thread and populates aTimerLabel and aTimerDuration, used by
  // CreateLogOrEndTimerValue.
  // * aCx - the JSContext rooting aName.
  // * aName - this is (should be) the name of the timer as JS::Value.
  // * aTimestamp - the monotonicTimer for this context taken from
  //                performance.now().
  // * aTimerLabel - This label will be populated with the aName converted to a
  //                 string.
  // * aTimerDuration - the difference between aTimestamp and when the timer
  //                    started (see StartTimer).
  // * aCancelTimer - if true, the timer is removed from the table.
  TimerStatus LogTimer(JSContext* aCx, const JS::Value& aName,
                       DOMHighResTimeStamp aTimestamp, nsAString& aTimerLabel,
                       double* aTimerDuration, bool aCancelTimer);

  // This method generates a ConsoleTimerEnd dictionary exposed as JS::Value, or
  // a ConsoleTimerError dictionary if aTimerStatus is false. See LogTimer.
  // * aCx - this is the context that will root the returned value.
  // * aTimerLabel - this label must be what LogTimer received as aTimerLabel.
  // * aTimerDuration - this is what LogTimer received as aTimerDuration
  // * aTimerStatus - the return value of LogTimer.
  JS::Value CreateLogOrEndTimerValue(JSContext* aCx,
                                     const nsAString& aTimerLabel,
                                     double aTimerDuration,
                                     TimerStatus aTimerStatus) const;

  // The method populates a Sequence from an array of JS::Value.
  bool ArgumentsToValueList(const Sequence<JS::Value>& aData,
                            Sequence<JS::Value>& aSequence) const;

  // This method follows the same pattern as StartTimer: its runs on the owning
  // thread and populate aCountLabel, used by CreateCounterOrResetCounterValue.
  // Returns 3 possible values:
  // * MAX_PAGE_COUNTERS in case of error that has to be reported;
  // * 0 in case of a CX exception. The operation cannot continue;
  // * the incremented counter value.
  // Params:
  // * aCx - the JSContext rooting aData.
  // * aData - the arguments received by the console.count() method.
  // * aCountLabel - the label that will be populated by this method.
  uint32_t IncreaseCounter(JSContext* aCx, const Sequence<JS::Value>& aData,
                           nsAString& aCountLabel);

  // This method follows the same pattern as StartTimer: its runs on the owning
  // thread and populate aCountLabel, used by CreateCounterResetValue. Returns
  // 3 possible values:
  // * MAX_PAGE_COUNTERS in case of error that has to be reported;
  // * 0 elsewhere. In case of a CX exception, aCountLabel will be an empty
  // string.
  // Params:
  // * aCx - the JSContext rooting aData.
  // * aData - the arguments received by the console.count() method.
  // * aCountLabel - the label that will be populated by this method.
  uint32_t ResetCounter(JSContext* aCx, const Sequence<JS::Value>& aData,
                        nsAString& aCountLabel);

  // This method generates a ConsoleCounter dictionary as JS::Value. If
  // aCountValue is == MAX_PAGE_COUNTERS it generates a ConsoleCounterError
  // instead. See IncreaseCounter.
  // * aCx - this is the context that will root the returned value.
  // * aCountLabel - this label must be what IncreaseCounter received as
  //                 aTimerLabel.
  // * aCountValue - the return value of IncreaseCounter.
  JS::Value CreateCounterOrResetCounterValue(JSContext* aCx,
                                             const nsAString& aCountLabel,
                                             uint32_t aCountValue) const;

  bool ShouldIncludeStackTrace(MethodName aMethodName) const;

  JSObject* GetOrCreateSandbox(JSContext* aCx, nsIPrincipal* aPrincipal);

  void AssertIsOnOwningThread() const;

  bool IsShuttingDown() const;

  bool MonotonicTimer(JSContext* aCx, MethodName aMethodName,
                      const Sequence<JS::Value>& aData,
                      DOMHighResTimeStamp* aTimeStamp);

  MOZ_CAN_RUN_SCRIPT
  void MaybeExecuteDumpFunction(JSContext* aCx, const nsAString& aMethodName,
                                const Sequence<JS::Value>& aData,
                                nsIStackFrame* aStack);

  MOZ_CAN_RUN_SCRIPT
  void MaybeExecuteDumpFunctionForTime(JSContext* aCx, MethodName aMethodName,
                                       const nsAString& aMethodString,
                                       uint64_t aMonotonicTimer,
                                       const JS::Value& aData);

  MOZ_CAN_RUN_SCRIPT
  void ExecuteDumpFunction(const nsAString& aMessage);

  bool IsEnabled(JSContext* aCx) const;

  bool ShouldProceed(MethodName aName) const;

  uint32_t WebIDLLogLevelToInteger(ConsoleLogLevel aLevel) const;

  uint32_t InternalLogLevelToInteger(MethodName aName) const;

  class ArgumentData {
   public:
    bool Initialize(JSContext* aCx, const Sequence<JS::Value>& aArguments);
    void Trace(const TraceCallbacks& aCallbacks, void* aClosure);
    bool PopulateArgumentsSequence(Sequence<JS::Value>& aSequence) const;
    JSObject* Global() const { return mGlobal; }

   private:
    void AssertIsOnOwningThread() const {
      NS_ASSERT_OWNINGTHREAD(ArgumentData);
    }

    NS_DECL_OWNINGTHREAD;
    JS::Heap<JSObject*> mGlobal;
    nsTArray<JS::Heap<JS::Value>> mArguments;
  };

  // Owning/CC thread only
  nsCOMPtr<nsIGlobalObject> mGlobal;
  // These nsCOMPtr are touched on main thread only.
  nsCOMPtr<nsIConsoleAPIStorage> mStorage;
  RefPtr<JSObjectHolder> mSandbox;

  // Touched on the owner thread.
  nsDataHashtable<nsStringHashKey, DOMHighResTimeStamp> mTimerRegistry;
  nsDataHashtable<nsStringHashKey, uint32_t> mCounterRegistry;

  nsTArray<RefPtr<ConsoleCallData>> mCallDataStorage;
  // These are references to the arguments we received in each call
  // from the DOM bindings.
  // Vector<T> supports non-memmovable types such as ArgumentData
  // (without any need to jump through hoops like
  // MOZ_DECLARE_RELOCATE_USING_MOVE_CONSTRUCTOR_FOR_TEMPLATE for nsTArray).
  Vector<ArgumentData> mArgumentStorage;

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
  nsString mPrefix;
  bool mChromeInstance;
  ConsoleLogLevel mMaxLogLevel;

  enum { eUnknown, eInitialized, eShuttingDown } mStatus;

  // This is used when Console is created and it's used only for JSM custom
  // console instance.
  mozilla::TimeStamp mCreationTimeStamp;

  friend class ConsoleCallData;
  friend class ConsoleCallDataWorkletRunnable;
  friend class ConsoleInstance;
  friend class ConsoleProfileWorkerRunnable;
  friend class ConsoleProfileWorkletRunnable;
  friend class ConsoleRunnable;
  friend class ConsoleWorkerRunnable;
  friend class ConsoleWorkletRunnable;
};

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_Console_h */
