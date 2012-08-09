/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Capabilities_h
#define mozilla_Capabilities_h

namespace mozilla {

namespace dom {
class PBrowserParent;
class PContentParent;
}

/**
 * Return true iff the specified browser has the specified capability.
 */
bool
AppProcessHasPermissions(mozilla::dom::PBrowserParent* aActor,
                         const char* aPermission);

/**
 * Return true iff any of the PBrowsers loaded in this content process
 * has the specified capability.
 */
bool
AppProcessHasPermission(mozilla::dom::PContentParent* aActor,
                        const char* aPermission);

// NB: when adding capability checks for other IPDL actors, please add
// them to this file and have them delegate to the two functions above
// as appropriate.  For example,
//
//   bool AppProcessHasCapability(PNeckoParent* aActor) {
//     return AppProcessHasCapability(aActor->Manager());
//   }

} // namespace mozilla

#endif // mozilla_Capabilities_h
