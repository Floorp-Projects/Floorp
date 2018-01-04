/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ConsoleInstance.h"
#include "mozilla/dom/ConsoleBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ConsoleInstance, mConsole)

NS_IMPL_CYCLE_COLLECTING_ADDREF(ConsoleInstance)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ConsoleInstance)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ConsoleInstance)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END

ConsoleInstance::ConsoleInstance(const ConsoleInstanceOptions& aOptions)
  : mConsole(new Console(nullptr))
{
  mConsole->mConsoleID = aOptions.mConsoleID;
}

ConsoleInstance::~ConsoleInstance()
{}

JSObject*
ConsoleInstance::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return ConsoleInstanceBinding::Wrap(aCx, this, aGivenProto);
}

#define METHOD(name, string)                                               \
  void                                                                     \
  ConsoleInstance::name(JSContext* aCx, const Sequence<JS::Value>& aData)  \
  {                                                                        \
    mConsole->MethodInternal(aCx, Console::Method##name,                   \
                             NS_LITERAL_STRING(string), aData);            \
  }

METHOD(Log, "log")
METHOD(Info, "info")
METHOD(Warn, "warn")
METHOD(Error, "error")
METHOD(Exception, "exception")
METHOD(Debug, "debug")
METHOD(Table, "table")
METHOD(Trace, "trace")
METHOD(Dir, "dir");
METHOD(Dirxml, "dirxml");
METHOD(Group, "group")
METHOD(GroupCollapsed, "groupCollapsed")

#undef METHOD

void
ConsoleInstance::GroupEnd(JSContext* aCx)
{
  const Sequence<JS::Value> data;
  mConsole->MethodInternal(aCx, Console::MethodGroupEnd,
                           NS_LITERAL_STRING("groupEnd"), data);
}

void
ConsoleInstance::Time(JSContext* aCx, const nsAString& aLabel)
{
  mConsole->StringMethodInternal(aCx, aLabel, Console::MethodTime,
                                 NS_LITERAL_STRING("time"));
}

void
ConsoleInstance::TimeEnd(JSContext* aCx, const nsAString& aLabel)
{
  mConsole->StringMethodInternal(aCx, aLabel, Console::MethodTimeEnd,
                                 NS_LITERAL_STRING("timeEnd"));
}

void
ConsoleInstance::TimeStamp(JSContext* aCx, const JS::Handle<JS::Value> aData)
{
  ClearException ce(aCx);

  Sequence<JS::Value> data;
  SequenceRooter<JS::Value> rooter(aCx, &data);

  if (aData.isString() && !data.AppendElement(aData, fallible)) {
    return;
  }

  mConsole->MethodInternal(aCx, Console::MethodTimeStamp,
                           NS_LITERAL_STRING("timeStamp"), data);
}

void
ConsoleInstance::Profile(JSContext* aCx, const Sequence<JS::Value>& aData)
{
  mConsole->ProfileMethodInternal(aCx, NS_LITERAL_STRING("profile"), aData);
}

void
ConsoleInstance::ProfileEnd(JSContext* aCx, const Sequence<JS::Value>& aData)
{
  mConsole->ProfileMethodInternal(aCx, NS_LITERAL_STRING("profileEnd"), aData);
}

void
ConsoleInstance::Assert(JSContext* aCx, bool aCondition,
                        const Sequence<JS::Value>& aData)
{
  if (!aCondition) {
    mConsole->MethodInternal(aCx, Console::MethodAssert,
                             NS_LITERAL_STRING("assert"), aData);
  }
}

void
ConsoleInstance::Count(JSContext* aCx, const nsAString& aLabel)
{
  mConsole->StringMethodInternal(aCx, aLabel, Console::MethodCount,
                                 NS_LITERAL_STRING("count"));
}

void
ConsoleInstance::Clear(JSContext* aCx)
{
  const Sequence<JS::Value> data;
  mConsole->MethodInternal(aCx, Console::MethodClear,
                           NS_LITERAL_STRING("clear"), data);
}

} // namespace dom
} // namespace mozilla
