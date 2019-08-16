/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_fetcheventopparent_h__
#define mozilla_dom_fetcheventopparent_h__

#include "nsISupports.h"

#include "mozilla/dom/PFetchEventOpParent.h"

namespace mozilla {
namespace dom {

class ServiceWorkerFetchEventOpArgs;

class FetchEventOpParent final : public PFetchEventOpParent {
  friend class PFetchEventOpParent;

 public:
  NS_INLINE_DECL_REFCOUNTING(FetchEventOpParent)

  FetchEventOpParent() = default;

  void Initialize(const ServiceWorkerFetchEventOpArgs& aArgs);

 private:
  ~FetchEventOpParent() = default;

  void ActorDestroy(ActorDestroyReason) override;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_fetcheventopparent_h__
