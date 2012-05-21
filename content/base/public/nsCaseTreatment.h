/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsCaseTreatment_h___
#define nsCaseTreatment_h___

/**
 * This is the enum used by functions that need to be told whether to
 * do case-sensitive or case-insensitive string comparisons.
 */
enum nsCaseTreatment {
  eCaseMatters,
  eIgnoreCase
};

#endif /* nsCaseTreatment_h___ */
