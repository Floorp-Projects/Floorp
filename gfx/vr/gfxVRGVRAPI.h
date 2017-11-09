/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_GVR_API_H
#define GFX_VR_GVR_API_H
namespace mozilla {
namespace gfx {

void SetGVRPresentingContext(void* aGVRPresentingContext);
void CleanupGVRNonPresentingContext();
void SetGVRPaused(const bool aPaused);

} // namespace gfx
} // namespace mozilla
#endif // GFX_VR_GVR_API_H
