/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSTEXTFRAMETEXTRUNCACHE_H_
#define NSTEXTFRAMETEXTRUNCACHE_H_

#include "nscore.h"

/**
 * This is implemented by both nsTextFrame.cpp and nsTextFrameThebes.cpp. Which
 * implementation gets used depends on which file gets built.
 */
class nsTextFrameTextRunCache {
public:
  static void Init();
  static void Shutdown();
};

#endif
