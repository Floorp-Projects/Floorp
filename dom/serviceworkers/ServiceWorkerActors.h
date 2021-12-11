/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_serviceworkeractors_h__
#define mozilla_dom_serviceworkeractors_h__

namespace mozilla {
namespace dom {

// PServiceWorker

class IPCServiceWorkerDescriptor;
class PServiceWorkerParent;

void InitServiceWorkerParent(PServiceWorkerParent* aActor,
                             const IPCServiceWorkerDescriptor& aDescriptor);

// PServiceWorkerContainer

class PServiceWorkerContainerParent;

void InitServiceWorkerContainerParent(PServiceWorkerContainerParent* aActor);

// PServiceWorkerRegistration

class IPCServiceWorkerRegistrationDescriptor;
class PServiceWorkerRegistrationParent;

void InitServiceWorkerRegistrationParent(
    PServiceWorkerRegistrationParent* aActor,
    const IPCServiceWorkerRegistrationDescriptor& aDescriptor);

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_serviceworkeractors_h__
