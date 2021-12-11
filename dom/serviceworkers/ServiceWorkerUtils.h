/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ServiceWorkerUtils_h
#define _mozilla_dom_ServiceWorkerUtils_h

#include "mozilla/MozPromise.h"
#include "mozilla/dom/IPCNavigationPreloadState.h"
#include "mozilla/dom/ServiceWorkerRegistrationDescriptor.h"
#include "nsTArray.h"

class nsIURI;

namespace mozilla {

class CopyableErrorResult;
class ErrorResult;

namespace dom {

class ClientInfo;
class ServiceWorkerRegistrationData;
class ServiceWorkerRegistrationDescriptor;
struct NavigationPreloadState;

using ServiceWorkerRegistrationPromise =
    MozPromise<ServiceWorkerRegistrationDescriptor, CopyableErrorResult, false>;

using ServiceWorkerRegistrationListPromise =
    MozPromise<CopyableTArray<ServiceWorkerRegistrationDescriptor>,
               CopyableErrorResult, false>;

using NavigationPreloadStatePromise =
    MozPromise<IPCNavigationPreloadState, CopyableErrorResult, false>;

using ServiceWorkerRegistrationCallback =
    std::function<void(const ServiceWorkerRegistrationDescriptor&)>;

using ServiceWorkerRegistrationListCallback =
    std::function<void(const nsTArray<ServiceWorkerRegistrationDescriptor>&)>;

using ServiceWorkerBoolCallback = std::function<void(bool)>;

using ServiceWorkerFailureCallback = std::function<void(ErrorResult&&)>;

using NavigationPreloadGetStateCallback =
    std::function<void(NavigationPreloadState&&)>;

bool ServiceWorkerRegistrationDataIsValid(
    const ServiceWorkerRegistrationData& aData);

void ServiceWorkerScopeAndScriptAreValid(const ClientInfo& aClientInfo,
                                         nsIURI* aScopeURI, nsIURI* aScriptURI,
                                         ErrorResult& aRv);

}  // namespace dom
}  // namespace mozilla

#endif  // _mozilla_dom_ServiceWorkerUtils_h
