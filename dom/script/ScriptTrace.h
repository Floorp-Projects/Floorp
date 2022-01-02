/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ScriptTrace_h
#define mozilla_dom_ScriptTrace_h

#include "ScriptLoader.h"

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/StaticPrefs_dom.h"

namespace mozilla {
namespace dom {
namespace script {

// This macro is used to wrap a tracing mechanism which is scheduling events
// which are then used by the JavaScript code of test cases to track the code
// path to verify the optimizations are working as expected.
#define TRACE_FOR_TEST(elem, str)                                              \
  PR_BEGIN_MACRO                                                               \
  nsresult rv = NS_OK;                                                         \
  rv =                                                                         \
      script::TestingDispatchEvent(elem, NS_LITERAL_STRING_FROM_CSTRING(str)); \
  NS_ENSURE_SUCCESS(rv, rv);                                                   \
  PR_END_MACRO

#define TRACE_FOR_TEST_BOOL(elem, str)                                         \
  PR_BEGIN_MACRO                                                               \
  nsresult rv = NS_OK;                                                         \
  rv =                                                                         \
      script::TestingDispatchEvent(elem, NS_LITERAL_STRING_FROM_CSTRING(str)); \
  NS_ENSURE_SUCCESS(rv, false);                                                \
  PR_END_MACRO

#define TRACE_FOR_TEST_NONE(elem, str)                                     \
  PR_BEGIN_MACRO                                                           \
  script::TestingDispatchEvent(elem, NS_LITERAL_STRING_FROM_CSTRING(str)); \
  PR_END_MACRO

static nsresult TestingDispatchEvent(nsIScriptElement* aScriptElement,
                                     const nsAString& aEventType) {
  if (!StaticPrefs::dom_expose_test_interfaces()) {
    return NS_OK;
  }

  nsCOMPtr<nsINode> target(do_QueryInterface(aScriptElement));
  if (!target) {
    return NS_OK;
  }

  RefPtr<AsyncEventDispatcher> dispatcher = new AsyncEventDispatcher(
      target, aEventType, CanBubble::eYes, ChromeOnlyDispatch::eNo);
  return dispatcher->PostDOMEvent();
}
}  // namespace script
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ScriptTrace_h
