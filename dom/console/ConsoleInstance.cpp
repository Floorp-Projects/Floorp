/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ConsoleInstance.h"
#include "mozilla/dom/ConsoleBinding.h"
#include "mozilla/Preferences.h"
#include "ConsoleCommon.h"
#include "ConsoleUtils.h"
#include "nsContentUtils.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ConsoleInstance, mConsole)

NS_IMPL_CYCLE_COLLECTING_ADDREF(ConsoleInstance)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ConsoleInstance)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ConsoleInstance)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END

namespace {

ConsoleUtils::Level WebIDLevelToConsoleUtilsLevel(ConsoleLevel aLevel) {
  switch (aLevel) {
    case ConsoleLevel::Log:
      return ConsoleUtils::eLog;
    case ConsoleLevel::Warning:
      return ConsoleUtils::eWarning;
    case ConsoleLevel::Error:
      return ConsoleUtils::eError;
    default:
      break;
  }

  return ConsoleUtils::eLog;
}

}  // namespace

ConsoleInstance::ConsoleInstance(JSContext* aCx,
                                 const ConsoleInstanceOptions& aOptions)
    : mConsole(new Console(aCx, nullptr, 0, 0)) {
  mConsole->mConsoleID = aOptions.mConsoleID;
  mConsole->mPassedInnerID = aOptions.mInnerID;

  if (aOptions.mDump.WasPassed()) {
    mConsole->mDumpFunction = &aOptions.mDump.Value();
  }

  mConsole->mPrefix = aOptions.mPrefix;

  // Let's inform that this is a custom instance.
  mConsole->mChromeInstance = true;

  if (aOptions.mMaxLogLevel.WasPassed()) {
    mConsole->mMaxLogLevel = aOptions.mMaxLogLevel.Value();
  }

  if (!aOptions.mMaxLogLevelPref.IsEmpty()) {
    mConsole->mMaxLogLevelPref = aOptions.mMaxLogLevelPref;
    NS_ConvertUTF16toUTF8 pref(aOptions.mMaxLogLevelPref);
    nsAutoCString value;
    nsresult rv = Preferences::GetCString(pref.get(), value);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      nsString message;
      message.AssignLiteral(
          "Console.maxLogLevelPref used with a non-existing pref: ");
      message.Append(aOptions.mMaxLogLevelPref);

      nsContentUtils::LogSimpleConsoleError(message, "chrome"_ns, false,
                                            true /* from chrome context*/);
    }
  }
}

ConsoleInstance::~ConsoleInstance() = default;

JSObject* ConsoleInstance::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return ConsoleInstance_Binding::Wrap(aCx, this, aGivenProto);
}

#define METHOD(name, string)                                     \
  void ConsoleInstance::name(JSContext* aCx,                     \
                             const Sequence<JS::Value>& aData) { \
    RefPtr<Console> console(mConsole);                           \
    console->MethodInternal(aCx, Console::Method##name,          \
                            nsLiteralString(string), aData);     \
  }

METHOD(Log, u"log")
METHOD(Info, u"info")
METHOD(Warn, u"warn")
METHOD(Error, u"error")
METHOD(Exception, u"exception")
METHOD(Debug, u"debug")
METHOD(Table, u"table")
METHOD(Trace, u"trace")
METHOD(Dir, u"dir");
METHOD(Dirxml, u"dirxml");
METHOD(Group, u"group")
METHOD(GroupCollapsed, u"groupCollapsed")

#undef METHOD

void ConsoleInstance::GroupEnd(JSContext* aCx) {
  const Sequence<JS::Value> data;
  RefPtr<Console> console(mConsole);
  console->MethodInternal(aCx, Console::MethodGroupEnd, u"groupEnd"_ns, data);
}

void ConsoleInstance::Time(JSContext* aCx, const nsAString& aLabel) {
  RefPtr<Console> console(mConsole);
  console->StringMethodInternal(aCx, aLabel, Sequence<JS::Value>(),
                                Console::MethodTime, u"time"_ns);
}

void ConsoleInstance::TimeLog(JSContext* aCx, const nsAString& aLabel,
                              const Sequence<JS::Value>& aData) {
  RefPtr<Console> console(mConsole);
  console->StringMethodInternal(aCx, aLabel, aData, Console::MethodTimeLog,
                                u"timeLog"_ns);
}

void ConsoleInstance::TimeEnd(JSContext* aCx, const nsAString& aLabel) {
  RefPtr<Console> console(mConsole);
  console->StringMethodInternal(aCx, aLabel, Sequence<JS::Value>(),
                                Console::MethodTimeEnd, u"timeEnd"_ns);
}

void ConsoleInstance::TimeStamp(JSContext* aCx,
                                const JS::Handle<JS::Value> aData) {
  ConsoleCommon::ClearException ce(aCx);

  Sequence<JS::Value> data;
  SequenceRooter<JS::Value> rooter(aCx, &data);

  if (aData.isString() && !data.AppendElement(aData, fallible)) {
    return;
  }

  RefPtr<Console> console(mConsole);
  console->MethodInternal(aCx, Console::MethodTimeStamp, u"timeStamp"_ns, data);
}

void ConsoleInstance::Profile(JSContext* aCx,
                              const Sequence<JS::Value>& aData) {
  RefPtr<Console> console(mConsole);
  console->ProfileMethodInternal(aCx, Console::MethodProfile, u"profile"_ns,
                                 aData);
}

void ConsoleInstance::ProfileEnd(JSContext* aCx,
                                 const Sequence<JS::Value>& aData) {
  RefPtr<Console> console(mConsole);
  console->ProfileMethodInternal(aCx, Console::MethodProfileEnd,
                                 u"profileEnd"_ns, aData);
}

void ConsoleInstance::Assert(JSContext* aCx, bool aCondition,
                             const Sequence<JS::Value>& aData) {
  if (!aCondition) {
    RefPtr<Console> console(mConsole);
    console->MethodInternal(aCx, Console::MethodAssert, u"assert"_ns, aData);
  }
}

void ConsoleInstance::Count(JSContext* aCx, const nsAString& aLabel) {
  RefPtr<Console> console(mConsole);
  console->StringMethodInternal(aCx, aLabel, Sequence<JS::Value>(),
                                Console::MethodCount, u"count"_ns);
}

void ConsoleInstance::CountReset(JSContext* aCx, const nsAString& aLabel) {
  RefPtr<Console> console(mConsole);
  console->StringMethodInternal(aCx, aLabel, Sequence<JS::Value>(),
                                Console::MethodCountReset, u"countReset"_ns);
}

void ConsoleInstance::Clear(JSContext* aCx) {
  const Sequence<JS::Value> data;
  RefPtr<Console> console(mConsole);
  console->MethodInternal(aCx, Console::MethodClear, u"clear"_ns, data);
}

void ConsoleInstance::ReportForServiceWorkerScope(const nsAString& aScope,
                                                  const nsAString& aMessage,
                                                  const nsAString& aFilename,
                                                  uint32_t aLineNumber,
                                                  uint32_t aColumnNumber,
                                                  ConsoleLevel aLevel) {
  if (!NS_IsMainThread()) {
    return;
  }

  ConsoleUtils::ReportForServiceWorkerScope(
      aScope, aMessage, aFilename, aLineNumber, aColumnNumber,
      WebIDLevelToConsoleUtilsLevel(aLevel));
}

}  // namespace mozilla::dom
