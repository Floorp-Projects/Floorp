/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TVManager_h__
#define mozilla_dom_TVManager_h__

#include "mozilla/DOMEventTargetHelper.h"

namespace mozilla {
namespace dom {

class Promise;

class TVManager MOZ_FINAL : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TVManager, DOMEventTargetHelper)

  explicit TVManager(nsPIDOMWindow* aWindow);

  // WebIDL (internal functions)

  virtual JSObject* WrapObject(JSContext *aCx) MOZ_OVERRIDE;

  // WebIDL (public APIs)

  already_AddRefed<Promise> GetTuners(ErrorResult& aRv);

private:
  ~TVManager();

};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TVManager_h__
