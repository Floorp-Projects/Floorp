/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_NativeUnderlyingSource_h
#define mozilla_dom_NativeUnderlyingSource_h

#include "js/TypeDecls.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/ErrorResult.h"
#include "nsCycleCollectionParticipant.h"

namespace mozilla::dom {
class Promise;
class ReadableStreamController;

// A class implementing NativeUnderlyingSource must be kept alive via some
// mechanism, but NativeUnderlyingSource -does not provide that mechanism-.
class NativeUnderlyingSource {
 public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  virtual already_AddRefed<Promise> PullCallback(
      JSContext* aCx, ReadableStreamController& aController,
      ErrorResult& aRv) = 0;

  virtual already_AddRefed<Promise> CancelCallback(
      JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aReason,
      ErrorResult& aRv) = 0;

  virtual void ErrorCallback() = 0;

 protected:
  virtual ~NativeUnderlyingSource() = default;
};

}  // namespace mozilla::dom

#endif
