/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ReportBody.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ReportBody, mWindow)
NS_IMPL_CYCLE_COLLECTING_ADDREF(ReportBody)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ReportBody)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ReportBody)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

ReportBody::ReportBody(nsPIDOMWindowInner* aWindow) : mWindow(aWindow) {
  MOZ_ASSERT(aWindow);
}

ReportBody::~ReportBody() = default;

}  // namespace dom
}  // namespace mozilla
