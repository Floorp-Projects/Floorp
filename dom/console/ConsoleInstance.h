/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ConsoleInstance_h
#define mozilla_dom_ConsoleInstance_h

#include "mozilla/dom/Console.h"


namespace mozilla {
namespace dom {

class ConsoleInstance final : public nsISupports
                            , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ConsoleInstance)

  explicit ConsoleInstance(JSContext* aCx,
                           const ConsoleInstanceOptions& aOptions);

  // WebIDL methods
  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  nsPIDOMWindowInner* GetParentObject() const
  {
    return nullptr;
  }

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
  Trace(JSContext* aCx, const Sequence<JS::Value>& aData);

  void
  Dir(JSContext* aCx, const Sequence<JS::Value>& aData);

  void
  Dirxml(JSContext* aCx, const Sequence<JS::Value>& aData);

  void
  Group(JSContext* aCx, const Sequence<JS::Value>& aData);

  void
  GroupCollapsed(JSContext* aCx, const Sequence<JS::Value>& aData);

  void
  GroupEnd(JSContext* aCx);

  void
  Time(JSContext* aCx, const nsAString& aLabel);

  void
  TimeLog(JSContext* aCx, const nsAString& aLabel,
          const Sequence<JS::Value>& aData);

  void
  TimeEnd(JSContext* aCx, const nsAString& aLabel);

  void
  TimeStamp(JSContext* aCx, const JS::Handle<JS::Value> aData);

  void
  Profile(JSContext* aCx, const Sequence<JS::Value>& aData);

  void
  ProfileEnd(JSContext* aCx, const Sequence<JS::Value>& aData);

  void
  Assert(JSContext* aCx, bool aCondition, const Sequence<JS::Value>& aData);

  void
  Count(JSContext* aCx, const nsAString& aLabel);

  void
  Clear(JSContext* aCx);

  // For testing only.
  void ReportForServiceWorkerScope(const nsAString& aScope,
                                   const nsAString& aMessage,
                                   const nsAString& aFilename,
                                   uint32_t aLineNumber,
                                   uint32_t aColumnNumber,
                                   ConsoleLevel aLevel);

private:
  ~ConsoleInstance();

  RefPtr<Console> mConsole;
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_ConsoleInstance_h
