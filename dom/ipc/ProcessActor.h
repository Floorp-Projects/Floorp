/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ProcessActor_h
#define mozilla_dom_ProcessActor_h

#include "nsWrapperCache.h"
#include "nsISupports.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/ErrorResult.h"
#include "nsIURI.h"
#include "nsString.h"
#include "mozilla/dom/JSActor.h"

namespace mozilla {
namespace dom {

// Common base class for Content{Parent, Child}.
class ProcessActor : public nsISupports {
 protected:
  virtual ~ProcessActor() = default;

  // Load the module for the named Content Actor and contruct it.
  // This method will not initialize the actor or set its manager,
  // which is handled by callers.
  void ConstructActor(const nsACString& aName, JS::MutableHandleObject aActor,
                      ErrorResult& aRv);
  virtual const nsAString& GetRemoteType() const = 0;
  virtual JSActor::Type GetSide() = 0;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ProcessActor_h
