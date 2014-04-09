/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsImageModule_h
#define nsImageModule_h

#include "nsError.h"

namespace mozilla {
namespace image {

nsresult InitModule();
void ShutdownModule();

} /* namespace image */
} /* namespace mozilla */


#endif /* nsImageModule_h */
