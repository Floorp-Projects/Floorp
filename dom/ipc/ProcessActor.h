/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ProcessActor_h
#define mozilla_dom_ProcessActor_h

#include "mozilla/dom/JSActorManager.h"
#include "nsStringFwd.h"

namespace mozilla {
class ErrorResult;

namespace dom {

class JSActorProtocol;
class JSActorService;

// Common base class for Content{Parent, Child} and InProcess{Parent, Child}.
class ProcessActor : public JSActorManager {
 protected:
  virtual ~ProcessActor() = default;

  already_AddRefed<JSActorProtocol> MatchingJSActorProtocol(
      JSActorService* aActorSvc, const nsACString& aName,
      ErrorResult& aRv) final;

  virtual const nsACString& GetRemoteType() const = 0;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ProcessActor_h
