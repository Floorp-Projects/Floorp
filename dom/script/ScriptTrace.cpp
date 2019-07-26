/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScriptTrace.h"
#include "mozilla/StaticPrefs_dom.h"

namespace mozilla {
namespace dom {
namespace script {

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
