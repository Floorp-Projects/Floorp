/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsmacattribution_h____
#define nsmacattribution_h____

#include "nsIMacAttribution.h"

class nsMacAttributionService : public nsIMacAttributionService {
 public:
  nsMacAttributionService(){};

  NS_DECL_ISUPPORTS
  NS_DECL_NSIMACATTRIBUTIONSERVICE

 protected:
  virtual ~nsMacAttributionService(){};
};

#endif  // nsmacattribution_h____
