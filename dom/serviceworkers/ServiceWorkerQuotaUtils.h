/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ServiceWorkerQuotaUtils_h
#define _mozilla_dom_ServiceWorkerQuotaUtils_h

#include <functional>

class nsIPrincipal;
class nsIQuotaUsageRequest;

namespace mozilla::dom {

using ServiceWorkerQuotaMitigationCallback = std::function<void(bool)>;

void ClearQuotaUsageIfNeeded(nsIPrincipal* aPrincipal,
                             ServiceWorkerQuotaMitigationCallback&& aCallback);

}  // namespace mozilla::dom

#endif
