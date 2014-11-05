/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGeoGridFuzzer_h
#define nsGeoGridFuzzer_h

#include "nsCOMPtr.h"
#include "nsIDOMGeoPosition.h"
#include "nsGeolocationSettings.h"

class nsGeoGridFuzzer MOZ_FINAL
{
public:

  static already_AddRefed<nsIDOMGeoPosition>
    FuzzLocation(const GeolocationSetting& aSetting, nsIDOMGeoPosition* aPosition);

private:
  nsGeoGridFuzzer() {} // can't construct
  nsGeoGridFuzzer(const nsGeoGridFuzzer&) {} // can't copy
};

#endif
