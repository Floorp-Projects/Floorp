/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessibleOrProxy.h"
#include "mozilla/a11y/DocAccessibleParent.h"
#include "mozilla/a11y/OuterDocAccessible.h"

namespace mozilla {
namespace a11y {

int32_t AccessibleOrProxy::IndexInParent() const {
  if (IsAccessible()) {
    return AsAccessible()->IndexInParent();
  }

  RemoteAccessible* proxy = AsProxy();
  if (!proxy) {
    return -1;
  }

  if (proxy->RemoteParent()) {
    return proxy->IndexInParent();
  }

  if (proxy->OuterDocOfRemoteBrowser()) {
    return 0;
  }

  MOZ_ASSERT_UNREACHABLE("Proxy should have parent or outer doc.");
  return -1;
}

AccessibleOrProxy AccessibleOrProxy::Parent() const {
  if (IsAccessible()) {
    return AsAccessible()->LocalParent();
  }

  RemoteAccessible* proxy = AsProxy();
  if (!proxy) {
    return nullptr;
  }

  if (RemoteAccessible* parent = proxy->RemoteParent()) {
    return parent;
  }

  // Otherwise this should be the proxy for the tab's top level document.
  return proxy->OuterDocOfRemoteBrowser();
}

AccessibleOrProxy AccessibleOrProxy::ChildAtPoint(
    int32_t aX, int32_t aY, Accessible::EWhichChildAtPoint aWhichChild) {
  // XXX: This implementation is silly now, but it will go away with AoP...
  Accessible* result = IsProxy()
                           ? AsProxy()->ChildAtPoint(aX, aY, aWhichChild)
                           : AsAccessible()->ChildAtPoint(aX, aY, aWhichChild);
  if (!result) {
    return nullptr;
  }

  if (result->IsLocal()) {
    return result->AsLocal();
  }

  if (result->IsRemote()) {
    return result->AsRemote();
  }

  return nullptr;
}

RemoteAccessible* AccessibleOrProxy::RemoteChildDoc() const {
  MOZ_ASSERT(!IsNull());
  if (IsProxy()) {
    return nullptr;
  }
  OuterDocAccessible* outerDoc = AsAccessible()->AsOuterDoc();
  if (!outerDoc) {
    return nullptr;
  }
  return outerDoc->RemoteChildDoc();
}

}  // namespace a11y
}  // namespace mozilla
