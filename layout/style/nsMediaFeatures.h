/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* the features that media queries can test */

#ifndef nsMediaFeatures_h_
#define nsMediaFeatures_h_

#include <stdint.h>
#include "nsCSSProps.h"

class nsMediaFeatures
{
public:
  static void InitSystemMetrics();
  static void FreeSystemMetrics();
  static void Shutdown();
};

#endif /* !defined(nsMediaFeatures_h_) */
