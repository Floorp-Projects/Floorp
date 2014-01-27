/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_Console_h
#define mozilla_dom_workers_Console_h

#include "Workers.h"
#include "WorkerPrivate.h"
#include "nsWrapperCache.h"

BEGIN_WORKERS_NAMESPACE

class ConsoleProxy;
class ConsoleStackData;

class WorkerConsole MOZ_FINAL : public nsWrapperCache
{
  WorkerConsole();

public:

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WorkerConsole)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WorkerConsole)

  static already_AddRefed<WorkerConsole>
  Create();

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  nsISupports* GetParentObject() const
  {
    return nullptr;
  }

  ~WorkerConsole();

  ConsoleProxy*
  GetProxy() const
  {
    return mProxy;
  }

  void
  SetProxy(ConsoleProxy* aProxy);

  // WebIDL methods

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
  Trace(JSContext* aCx);

  void
  Dir(JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aValue);

  void
  Group(JSContext* aCx, const Sequence<JS::Value>& aData);

  void
  GroupCollapsed(JSContext* aCx, const Sequence<JS::Value>& aData);

  void
  GroupEnd(JSContext* aCx, const Sequence<JS::Value>& aData);

  void
  Time(JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aTimer);

  void
  TimeEnd(JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aTimer);

  void
  Profile(JSContext* aCx, const Sequence<JS::Value>& aData);

  void
  ProfileEnd(JSContext* aCx, const Sequence<JS::Value>& aData);

  void
  Assert(JSContext* aCx, bool aCondition, const Sequence<JS::Value>& aData);

  void
  __noSuchMethod__();

private:
  void
  Method(JSContext* aCx, const char* aMethodName,
         const Sequence<JS::Value>& aData, uint32_t aMaxStackDepth);

  nsRefPtr<ConsoleProxy> mProxy;
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_Console_h
