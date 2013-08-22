/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDirection_h___
#define nsDirection_h___

// This file makes the nsDirection enum present both in nsIFrame.h and
// nsISelectionPrivate.h.

enum nsDirection {
  eDirNext    = 0,
  eDirPrevious= 1
};

#endif

