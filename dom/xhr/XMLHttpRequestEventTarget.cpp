/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XMLHttpRequestEventTarget.h"

#include "mozilla/dom/DebuggerNotificationBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(XMLHttpRequestEventTarget)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(XMLHttpRequestEventTarget,
                                                  DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(XMLHttpRequestEventTarget,
                                                DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(XMLHttpRequestEventTarget)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(XMLHttpRequestEventTarget, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(XMLHttpRequestEventTarget, DOMEventTargetHelper)

mozilla::Maybe<EventCallbackDebuggerNotificationType>
XMLHttpRequestEventTarget::GetDebuggerNotificationType() const {
  return mozilla::Some(EventCallbackDebuggerNotificationType::Xhr);
}

void XMLHttpRequestEventTarget::DisconnectFromOwner() {
  DOMEventTargetHelper::DisconnectFromOwner();
}

}  // namespace dom
}  // namespace mozilla
