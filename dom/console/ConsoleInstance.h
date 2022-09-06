/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ConsoleInstance_h
#define mozilla_dom_ConsoleInstance_h

#include "mozilla/dom/Console.h"

namespace mozilla::dom {

class ConsoleInstance final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(ConsoleInstance)

  explicit ConsoleInstance(JSContext* aCx,
                           const ConsoleInstanceOptions& aOptions);

  // WebIDL methods
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  nsPIDOMWindowInner* GetParentObject() const { return nullptr; }

  MOZ_CAN_RUN_SCRIPT
  void Log(JSContext* aCx, const Sequence<JS::Value>& aData);

  MOZ_CAN_RUN_SCRIPT
  void Info(JSContext* aCx, const Sequence<JS::Value>& aData);

  MOZ_CAN_RUN_SCRIPT
  void Warn(JSContext* aCx, const Sequence<JS::Value>& aData);

  MOZ_CAN_RUN_SCRIPT
  void Error(JSContext* aCx, const Sequence<JS::Value>& aData);

  MOZ_CAN_RUN_SCRIPT
  void Exception(JSContext* aCx, const Sequence<JS::Value>& aData);

  MOZ_CAN_RUN_SCRIPT
  void Debug(JSContext* aCx, const Sequence<JS::Value>& aData);

  MOZ_CAN_RUN_SCRIPT
  void Table(JSContext* aCx, const Sequence<JS::Value>& aData);

  MOZ_CAN_RUN_SCRIPT
  void Trace(JSContext* aCx, const Sequence<JS::Value>& aData);

  MOZ_CAN_RUN_SCRIPT
  void Dir(JSContext* aCx, const Sequence<JS::Value>& aData);

  MOZ_CAN_RUN_SCRIPT
  void Dirxml(JSContext* aCx, const Sequence<JS::Value>& aData);

  MOZ_CAN_RUN_SCRIPT
  void Group(JSContext* aCx, const Sequence<JS::Value>& aData);

  MOZ_CAN_RUN_SCRIPT
  void GroupCollapsed(JSContext* aCx, const Sequence<JS::Value>& aData);

  MOZ_CAN_RUN_SCRIPT
  void GroupEnd(JSContext* aCx);

  MOZ_CAN_RUN_SCRIPT
  void Time(JSContext* aCx, const nsAString& aLabel);

  MOZ_CAN_RUN_SCRIPT
  void TimeLog(JSContext* aCx, const nsAString& aLabel,
               const Sequence<JS::Value>& aData);

  MOZ_CAN_RUN_SCRIPT
  void TimeEnd(JSContext* aCx, const nsAString& aLabel);

  MOZ_CAN_RUN_SCRIPT
  void TimeStamp(JSContext* aCx, const JS::Handle<JS::Value> aData);

  MOZ_CAN_RUN_SCRIPT
  void Profile(JSContext* aCx, const Sequence<JS::Value>& aData);

  MOZ_CAN_RUN_SCRIPT
  void ProfileEnd(JSContext* aCx, const Sequence<JS::Value>& aData);

  MOZ_CAN_RUN_SCRIPT
  void Assert(JSContext* aCx, bool aCondition,
              const Sequence<JS::Value>& aData);

  MOZ_CAN_RUN_SCRIPT
  void Count(JSContext* aCx, const nsAString& aLabel);

  MOZ_CAN_RUN_SCRIPT
  void CountReset(JSContext* aCx, const nsAString& aLabel);

  MOZ_CAN_RUN_SCRIPT
  void Clear(JSContext* aCx);

  // For testing only.
  void ReportForServiceWorkerScope(const nsAString& aScope,
                                   const nsAString& aMessage,
                                   const nsAString& aFilename,
                                   uint32_t aLineNumber, uint32_t aColumnNumber,
                                   ConsoleLevel aLevel);

 private:
  ~ConsoleInstance();

  RefPtr<Console> mConsole;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_ConsoleInstance_h
