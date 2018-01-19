/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ConsoleInstance.h"
#include "mozilla/dom/ConsoleBinding.h"
#include "ConsoleCommon.h"
#include "ConsoleUtils.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ConsoleInstance, mConsole)

NS_IMPL_CYCLE_COLLECTING_ADDREF(ConsoleInstance)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ConsoleInstance)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ConsoleInstance)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END

namespace {

ConsoleLogLevel
PrefToValue(const nsCString& aPref)
{
  if (!NS_IsMainThread()) {
    NS_WARNING("Console.maxLogLevelPref is not supported on workers!");
    return ConsoleLogLevel::All;
  }

  nsAutoCString value;
  nsresult rv = Preferences::GetCString(aPref.get(), value);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return ConsoleLogLevel::All;
  }

  int index = FindEnumStringIndexImpl(value.get(), value.Length(),
                                      ConsoleLogLevelValues::strings);
  if (NS_WARN_IF(index < 0)) {
    return ConsoleLogLevel::All;
  }

  MOZ_ASSERT(index < (int)ConsoleLogLevel::EndGuard_);
  return static_cast<ConsoleLogLevel>(index);
}

ConsoleUtils::Level
WebIDLevelToConsoleUtilsLevel(ConsoleLevel aLevel)
{
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

} // anonymous

ConsoleInstance::ConsoleInstance(const ConsoleInstanceOptions& aOptions)
  : mConsole(new Console(nullptr))
{
  mConsole->mConsoleID = aOptions.mConsoleID;
  mConsole->mPassedInnerID = aOptions.mInnerID;

  if (aOptions.mDump.WasPassed()) {
    mConsole->mDumpFunction = &aOptions.mDump.Value();
  } else {
    // For historical reasons, ConsoleInstance prints messages on stdout.
    mConsole->mDumpToStdout = true;
  }

  mConsole->mPrefix = aOptions.mPrefix;

  // Let's inform that this is a custom instance.
  mConsole->mChromeInstance = true;

  if (aOptions.mMaxLogLevel.WasPassed()) {
    mConsole->mMaxLogLevel = aOptions.mMaxLogLevel.Value();
  }

  if (!aOptions.mMaxLogLevelPref.IsEmpty()) {
    mConsole->mMaxLogLevel =
      PrefToValue(NS_ConvertUTF16toUTF8(aOptions.mMaxLogLevelPref));
  }
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
  ConsoleCommon::ClearException ce(aCx);

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
  mConsole->ProfileMethodInternal(aCx, Console::MethodProfile,
                                  NS_LITERAL_STRING("profile"), aData);
}

void
ConsoleInstance::ProfileEnd(JSContext* aCx, const Sequence<JS::Value>& aData)
{
  mConsole->ProfileMethodInternal(aCx, Console::MethodProfileEnd,
                                  NS_LITERAL_STRING("profileEnd"), aData);
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

void
ConsoleInstance::ReportForServiceWorkerScope(const nsAString& aScope,
                                             const nsAString& aMessage,
                                             const nsAString& aFilename,
                                             uint32_t aLineNumber,
                                             uint32_t aColumnNumber,
                                             ConsoleLevel aLevel)
{
  if (!NS_IsMainThread()) {
    return;
  }

  ConsoleUtils::ReportForServiceWorkerScope(aScope, aMessage, aFilename,
                                            aLineNumber, aColumnNumber,
                                            WebIDLevelToConsoleUtilsLevel(aLevel));
}

} // namespace dom
} // namespace mozilla
