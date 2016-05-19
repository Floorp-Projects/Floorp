/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_build_nsImageModule_h
#define mozilla_image_build_nsImageModule_h

#include "nsError.h"

namespace mozilla {
namespace image {

nsresult EnsureModuleInitialized();
void ShutdownModule();

} /* namespace image */
} /* namespace mozilla */

#endif // mozilla_image_build_nsImageModule_h
