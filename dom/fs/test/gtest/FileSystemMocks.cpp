/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemMocks.h"

#include <string>

#include "ErrorList.h"
#include "gtest/gtest-assertion-result.h"
#include "js/RootingAPI.h"
#include "jsapi.h"
#include "mozilla/dom/FileSystemManager.h"
#include "nsContentUtils.h"
#include "nsISupports.h"

namespace testing::internal {

GTEST_API_ ::testing::AssertionResult CmpHelperSTREQ(const char* s1_expression,
                                                     const char* s2_expression,
                                                     const nsAString& s1,
                                                     const nsAString& s2) {
  if (s1.Equals(s2)) {
    return ::testing::AssertionSuccess();
  }

  return ::testing::internal::EqFailure(
      s1_expression, s2_expression,
      std::string(NS_ConvertUTF16toUTF8(s1).get()),
      std::string(NS_ConvertUTF16toUTF8(s2).get()),
      /* ignore case */ false);
}

}  // namespace testing::internal

namespace mozilla::dom::fs::test {

nsIGlobalObject* GetGlobal() {
  AutoJSAPI jsapi;
  DebugOnly<bool> ok = jsapi.Init(xpc::PrivilegedJunkScope());
  MOZ_ASSERT(ok);

  JSContext* cx = jsapi.cx();
  mozilla::dom::GlobalObject globalObject(cx, JS::CurrentGlobalOrNull(cx));
  nsCOMPtr<nsIGlobalObject> global =
      do_QueryInterface(globalObject.GetAsSupports());
  MOZ_ASSERT(global);

  return global.get();
}

nsresult GetAsString(const RefPtr<Promise>& aPromise, nsAString& aString) {
  AutoJSAPI jsapi;
  DebugOnly<bool> ok = jsapi.Init(xpc::PrivilegedJunkScope());
  MOZ_ASSERT(ok);

  JSContext* cx = jsapi.cx();

  JS::Rooted<JSObject*> promiseObj(cx, aPromise->PromiseObj());
  JS::Rooted<JS::Value> vp(cx, JS::GetPromiseResult(promiseObj));

  switch (aPromise->State()) {
    case Promise::PromiseState::Pending: {
      return NS_ERROR_DOM_INVALID_STATE_ERR;
    }

    case Promise::PromiseState::Resolved: {
      if (nsContentUtils::StringifyJSON(cx, &vp, aString)) {
        return NS_OK;
      }

      return NS_ERROR_UNEXPECTED;
    }

    case Promise::PromiseState::Rejected: {
      if (vp.isInt32()) {
        int32_t errorCode = vp.toInt32();
        aString.AppendInt(errorCode);

        return NS_OK;
      }

      if (!vp.isObject()) {
        return NS_ERROR_UNEXPECTED;
      }

      RefPtr<Exception> exception;
      UNWRAP_OBJECT(Exception, &vp, exception);
      if (!exception) {
        return NS_ERROR_UNEXPECTED;
      }

      aString.Append(NS_ConvertUTF8toUTF16(
          GetStaticErrorName(static_cast<nsresult>(exception->Result()))));

      return NS_OK;
    }

    default:
      break;
  }

  return NS_ERROR_FAILURE;
}

mozilla::ipc::PrincipalInfo GetPrincipalInfo() {
  return mozilla::ipc::PrincipalInfo{mozilla::ipc::SystemPrincipalInfo{}};
}

}  // namespace mozilla::dom::fs::test
