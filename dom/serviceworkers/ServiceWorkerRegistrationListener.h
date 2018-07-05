/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ServiceWorkerRegistrationListener_h
#define mozilla_dom_ServiceWorkerRegistrationListener_h

namespace mozilla {
namespace dom {

class ServiceWorkerRegistrationDescriptor;

// Used by ServiceWorkerManager to notify ServiceWorkerRegistrations of
// updatefound event and invalidating ServiceWorker instances.
class ServiceWorkerRegistrationListener
{
public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  virtual void
  UpdateState(const ServiceWorkerRegistrationDescriptor& aDescriptor) = 0;

  virtual void
  RegistrationRemoved() = 0;

  virtual void
  GetScope(nsAString& aScope) const = 0;

  virtual bool
  MatchesDescriptor(const ServiceWorkerRegistrationDescriptor& aDescriptor) = 0;
};


} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_ServiceWorkerRegistrationListener_h */
