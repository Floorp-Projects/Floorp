/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AvailabilityCollection_h
#define mozilla_dom_AvailabilityCollection_h

#include "mozilla/StaticPtr.h"
#include "mozilla/WeakPtr.h"
#include "nsString.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {

class PresentationAvailability;

class AvailabilityCollection final
{
public:
  static AvailabilityCollection* GetSingleton();

  void Add(PresentationAvailability* aAvailability);

  void Remove(PresentationAvailability* aAvailability);

  already_AddRefed<PresentationAvailability>
  Find(const uint64_t aWindowId, const nsAString& aUrl);

private:
  friend class StaticAutoPtr<AvailabilityCollection>;

  AvailabilityCollection();
  virtual ~AvailabilityCollection();

  static StaticAutoPtr<AvailabilityCollection> sSingleton;
  nsTArray<WeakPtr<PresentationAvailability>> mAvailabilities;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_AvailabilityCollection_h
