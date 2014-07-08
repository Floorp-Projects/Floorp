/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_backgroundutils_h__
#define mozilla_ipc_backgroundutils_h__

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nscore.h"

class nsIPrincipal;

namespace mozilla {
namespace ipc {

class PrincipalInfo;

/**
 * Convert a PrincipalInfo to an nsIPrincipal.
 *
 * MUST be called on the main thread only.
 */
already_AddRefed<nsIPrincipal>
PrincipalInfoToPrincipal(const PrincipalInfo& aPrincipalInfo,
                         nsresult* aOptionalResult = nullptr);

/**
 * Convert an nsIPrincipal to a PrincipalInfo.
 *
 * MUST be called on the main thread only.
 */
nsresult
PrincipalToPrincipalInfo(nsIPrincipal* aPrincipal,
                         PrincipalInfo* aPrincipalInfo);

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_backgroundutils_h__
