/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ServiceWorkerUtils_h
#define _mozilla_dom_ServiceWorkerUtils_h

#include "mozilla/MozPromise.h"
#include "mozilla/dom/ServiceWorkerRegistrationDescriptor.h"
#include "nsTArray.h"

namespace mozilla {

class ErrorResult;

namespace dom {

class ServiceWorkerRegistrationData;
class ServiceWorkerRegistrationDescriptor;

// Note: These are exclusive promise types.  Only one Then() or ChainTo()
//       call is allowed.  This is necessary since ErrorResult cannot
//       be copied.

typedef MozPromise<ServiceWorkerRegistrationDescriptor, ErrorResult, true>
        ServiceWorkerRegistrationPromise;

typedef MozPromise<nsTArray<ServiceWorkerRegistrationDescriptor>, ErrorResult, true>
        ServiceWorkerRegistrationListPromise;

bool
ServiceWorkerParentInterceptEnabled();

bool
ServiceWorkerRegistrationDataIsValid(const ServiceWorkerRegistrationData& aData);

} // namespace dom
} // namespace mozilla

#endif // _mozilla_dom_ServiceWorkerUtils_h
