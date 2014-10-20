/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Console_h
#define mozilla_dom_Console_h

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/ErrorResult.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsIObserver.h"
#include "nsITimer.h"
#include "nsWrapperCache.h"
#include "nsDOMNavigationTiming.h"
#include "nsPIDOMWindow.h"

class nsIConsoleAPIStorage;

namespace mozilla {
namespace dom {

class ConsoleCallData;
struct ConsoleStackEntry;

class Console MOZ_FINAL : public nsITimerCallback
                        , public nsIObserver
                        , public nsWrapperCache
{
  ~Console();

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(Console,
                                                         nsITimerCallback)
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSIOBSERVER

  explicit Console(nsPIDOMWindow* aWindow);

  // WebIDL methods
  nsISupports* GetParentObject() const
  {
    return mWindow;
  }

  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  void
  Log(JSContext* aCx, const Sequence<JS::Value>& aData);

  void
  Info(JSContext* aCx, const Sequence<JS::Value>& aData);

  void
  Warn(JSContext* aCx, const Sequence<JS::Value>& aData);

  void
  Error(JSContext* aCx, const Sequence<JS::Value>& aData);

  void
  Exception(JSContext* aCx, const Sequence<JS::Value>& aData);

  void
  Debug(JSContext* aCx, const Sequence<JS::Value>& aData);

  void
  Table(JSContext* aCx, const Sequence<JS::Value>& aData);

  void
  Trace(JSContext* aCx);

  void
  Dir(JSContext* aCx, const Sequence<JS::Value>& aData);

  void
  Group(JSContext* aCx, const Sequence<JS::Value>& aData);

  void
  GroupCollapsed(JSContext* aCx, const Sequence<JS::Value>& aData);

  void
  GroupEnd(JSContext* aCx, const Sequence<JS::Value>& aData);

  void
  Time(JSContext* aCx, const JS::Handle<JS::Value> aTime);

  void
  TimeEnd(JSContext* aCx, const JS::Handle<JS::Value> aTime);

  void
  Profile(JSContext* aCx, const Sequence<JS::Value>& aData);

  void
  ProfileEnd(JSContext* aCx, const Sequence<JS::Value>& aData);

  void
  Assert(JSContext* aCx, bool aCondition, const Sequence<JS::Value>& aData);

  void
  Count(JSContext* aCx, const Sequence<JS::Value>& aData);

  void
  __noSuchMethod__();

private:
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
    MethodGroup,
    MethodGroupCollapsed,
    MethodGroupEnd,
    MethodTime,
    MethodTimeEnd,
    MethodAssert,
    MethodCount
  };

  void
  Method(JSContext* aCx, MethodName aName, const nsAString& aString,
         const Sequence<JS::Value>& aData);

  void
  AppendCallData(ConsoleCallData* aData);

  void
  ProcessCallData(ConsoleCallData* aData);

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
  void
  ProcessArguments(JSContext* aCx, const nsTArray<JS::Heap<JS::Value>>& aData,
                   Sequence<JS::Value>& aSequence,
                   Sequence<JS::Value>& aStyles);

  void
  MakeFormatString(nsCString& aFormat, int32_t aInteger, int32_t aMantissa,
                   char aCh);

  // Stringify and Concat all the JS::Value in a single string using ' ' as
  // separator.
  void
  ComposeGroupName(JSContext* aCx, const nsTArray<JS::Heap<JS::Value>>& aData,
                   nsAString& aName);

  JS::Value
  StartTimer(JSContext* aCx, const JS::Value& aName,
             DOMHighResTimeStamp aTimestamp);

  JS::Value
  StopTimer(JSContext* aCx, const JS::Value& aName,
            DOMHighResTimeStamp aTimestamp);

  // The method populates a Sequence from an array of JS::Value.
  void
  ArgumentsToValueList(const nsTArray<JS::Heap<JS::Value>>& aData,
                       Sequence<JS::Value>& aSequence);

  void
  ProfileMethod(JSContext* aCx, const nsAString& aAction,
                const Sequence<JS::Value>& aData);

  JS::Value
  IncreaseCounter(JSContext* aCx, const ConsoleStackEntry& aFrame,
                   const nsTArray<JS::Heap<JS::Value>>& aArguments);

  void
  ClearConsoleData();

  bool
  ShouldIncludeStackrace(MethodName aMethodName);

  nsCOMPtr<nsPIDOMWindow> mWindow;
  nsCOMPtr<nsITimer> mTimer;
  nsCOMPtr<nsIConsoleAPIStorage> mStorage;

  LinkedList<ConsoleCallData> mQueuedCalls;
  nsDataHashtable<nsStringHashKey, DOMHighResTimeStamp> mTimerRegistry;
  nsDataHashtable<nsStringHashKey, uint32_t> mCounterRegistry;

  uint64_t mOuterID;
  uint64_t mInnerID;

  friend class ConsoleCallData;
  friend class ConsoleCallDataRunnable;
  friend class ConsoleProfileRunnable;
};

} // dom namespace
} // mozilla namespace

#endif /* mozilla_dom_Console_h */
