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

AccessibleOrProxy AccessibleOrProxy::Parent() const {
  if (IsAccessible()) {
    return AsAccessible()->Parent();
  }

  ProxyAccessible* proxy = AsProxy();
  if (!proxy) {
    return nullptr;
  }

  if (ProxyAccessible* parent = proxy->Parent()) {
    return parent;
  }

  // Otherwise this should be the proxy for the tab's top level document.
  return proxy->OuterDocOfRemoteBrowser();
}

AccessibleOrProxy AccessibleOrProxy::ChildAtPoint(
    int32_t aX, int32_t aY, Accessible::EWhichChildAtPoint aWhichChild) {
  if (IsProxy()) {
    return AsProxy()->ChildAtPoint(aX, aY, aWhichChild);
  }
  ProxyAccessible* childDoc = RemoteChildDoc();
  if (childDoc) {
    // This is an OuterDocAccessible.
    nsIntRect docRect = AsAccessible()->Bounds();
    if (!docRect.Contains(aX, aY)) {
      return nullptr;
    }
    if (aWhichChild == Accessible::eDirectChild) {
      return childDoc;
    }
    return childDoc->ChildAtPoint(aX, aY, aWhichChild);
  }
  AccessibleOrProxy target = AsAccessible()->ChildAtPoint(aX, aY, aWhichChild);
  MOZ_ASSERT(target.IsAccessible());
  if (aWhichChild == Accessible::eDirectChild) {
    return target;
  }
  childDoc = target.RemoteChildDoc();
  if (childDoc) {
    // Accessible::ChildAtPoint stopped at an OuterDocAccessible, since it
    // can't traverse into ProxyAccessibles. Continue the search from childDoc.
    return childDoc->ChildAtPoint(aX, aY, aWhichChild);
  }
  return target;
}

ProxyAccessible* AccessibleOrProxy::RemoteChildDoc() const {
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
