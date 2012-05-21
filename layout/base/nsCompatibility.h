/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* constants for quirks mode, standards mode, and almost standards mode */

#ifndef nsCompatibility_h___
#define nsCompatibility_h___

enum nsCompatibility {
  eCompatibility_FullStandards   = 1,
  eCompatibility_AlmostStandards = 2,
  eCompatibility_NavQuirks       = 3
};

#endif /* nsCompatibility_h___ */
