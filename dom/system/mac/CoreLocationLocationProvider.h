/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsIGeolocationProvider.h"

/*
 * The CoreLocationObjects class contains the CoreLocation objects
 * we'll need.
 *
 * Declaring them directly in CoreLocationLocationProvider
 * would require Objective-C++ syntax, which would contaminate all
 * files that include this header and require them to be Objective-C++
 * as well.
 *
 * The solution then is to forward-declare CoreLocationObjects here and
 * hold a pointer to it in CoreLocationLocationProvider, and only actually
 * define it in CoreLocationLocationProvider.mm, thus making it safe
 * for nsGeolocation.cpp, which is C++-only, to include this header.
 */
class CoreLocationObjects;

class CoreLocationLocationProvider
  : public nsIGeolocationProvider
  , public nsIGeolocationUpdate
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGEOLOCATIONUPDATE
  NS_DECL_NSIGEOLOCATIONPROVIDER

  CoreLocationLocationProvider();
  static bool IsCoreLocationAvailable();

private:
  virtual ~CoreLocationLocationProvider() {};

  CoreLocationObjects* mCLObjects;
  nsCOMPtr<nsIGeolocationUpdate> mCallback;
};
