/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ConsoleUtils.h"
#include "ConsoleCommon.h"

#include "mozilla/ClearOnShutdown.h"
#include "NullPrincipal.h"

namespace mozilla {
namespace dom {

namespace {

StaticRefPtr<ConsoleUtils> gConsoleUtilsService;

}

/* static */ ConsoleUtils*
ConsoleUtils::GetOrCreate()
{
  if (!gConsoleUtilsService) {
    MOZ_ASSERT(NS_IsMainThread());

    gConsoleUtilsService = new ConsoleUtils();
    ClearOnShutdown(&gConsoleUtilsService);
  }

  return gConsoleUtilsService;
}

ConsoleUtils::ConsoleUtils() = default;
ConsoleUtils::~ConsoleUtils() = default;

/* static */ void
ConsoleUtils::ReportForServiceWorkerScope(const nsAString& aScope,
                                          const nsAString& aMessage,
                                          const nsAString& aFilename,
                                          uint32_t aLineNumber,
                                          uint32_t aColumnNumber,
                                          Level aLevel)
{
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<ConsoleUtils> service = ConsoleUtils::GetOrCreate();
  if (NS_WARN_IF(!service)) {
    return;
  }

  service->ReportForServiceWorkerScopeInternal(aScope, aMessage, aFilename,
                                               aLineNumber, aColumnNumber,
                                               aLevel);
}

void
ConsoleUtils::ReportForServiceWorkerScopeInternal(const nsAString& aScope,
                                                  const nsAString& aMessage,
                                                  const nsAString& aFilename,
                                                  uint32_t aLineNumber,
                                                  uint32_t aColumnNumber,
                                                  Level aLevel)
{
  MOZ_ASSERT(NS_IsMainThread());

  AutoJSAPI jsapi;
  jsapi.Init();

  JSContext* cx = jsapi.cx();

  ConsoleCommon::ClearException ce(cx);
  JS::Rooted<JSObject*> global(cx, GetOrCreateSandbox(cx));
  if (NS_WARN_IF(!global)) {
    return;
  }

  // The GetOrCreateSandbox call returns a proxy to the actual sandbox object.
  // We don't need a proxy here.
  global = js::UncheckedUnwrap(global);

  JSAutoCompartment ac(cx, global);

  RootedDictionary<ConsoleEvent> event(cx);

  event.mID.Construct();
  event.mID.Value().SetAsString() = aScope;

  event.mInnerID.Construct();
  event.mInnerID.Value().SetAsString() = NS_LITERAL_STRING("ServiceWorker");

  switch (aLevel) {
    case eLog:
      event.mLevel = NS_LITERAL_STRING("log");
      break;

    case eWarning:
      event.mLevel = NS_LITERAL_STRING("warn");
      break;

    case eError:
      event.mLevel = NS_LITERAL_STRING("error");
      break;
  }

  event.mFilename = aFilename;
  event.mLineNumber = aLineNumber;
  event.mColumnNumber = aColumnNumber;
  event.mTimeStamp = JS_Now() / PR_USEC_PER_MSEC;

  JS::Rooted<JS::Value> messageValue(cx);
  if (!dom::ToJSValue(cx, aMessage, &messageValue)) {
    return;
  }

  event.mArguments.Construct();
  if (!event.mArguments.Value().AppendElement(messageValue, fallible)) {
    return;
  }

  nsCOMPtr<nsIConsoleAPIStorage> storage =
    do_GetService("@mozilla.org/consoleAPI-storage;1");

  if (NS_WARN_IF(!storage)) {
    return;
  }

  JS::Rooted<JS::Value> eventValue(cx);
  if (!ToJSValue(cx, event, &eventValue)) {
    return;
  }

  // This is a legacy property.
  JS::Rooted<JSObject*> eventObj(cx, &eventValue.toObject());
  if (NS_WARN_IF(!JS_DefineProperty(cx, eventObj, "wrappedJSObject", eventObj,
                                    JSPROP_ENUMERATE))) {
    return;
  }

  storage->RecordEvent(NS_LITERAL_STRING("ServiceWorker"), aScope, eventValue);
}

JSObject*
ConsoleUtils::GetOrCreateSandbox(JSContext* aCx)
{
  AssertIsOnMainThread();

  if (!mSandbox) {
    nsIXPConnect* xpc = nsContentUtils::XPConnect();
    MOZ_ASSERT(xpc, "This should never be null!");

    RefPtr<NullPrincipal> nullPrincipal = NullPrincipal::Create();

    JS::Rooted<JSObject*> sandbox(aCx);
    nsresult rv = xpc->CreateSandbox(aCx, nullPrincipal, sandbox.address());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }

    mSandbox = new JSObjectHolder(aCx, sandbox);
  }

  return mSandbox->GetJSObject();
}

} // namespace dom
} // namespace mozilla
