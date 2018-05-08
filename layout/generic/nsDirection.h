/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDirection_h___
#define nsDirection_h___

// This file makes the nsDirection enum present both in nsIFrame.h and
// Selection.h.

enum nsDirection {
  eDirNext    = 0,
  eDirPrevious= 1
};

#endif

