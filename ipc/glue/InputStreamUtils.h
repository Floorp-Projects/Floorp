/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_InputStreamUtils_h
#define mozilla_ipc_InputStreamUtils_h

#include "mozilla/ipc/InputStreamParams.h"
#include "nsCOMPtr.h"
#include "nsIInputStream.h"

namespace mozilla {
namespace ipc {

void
SerializeInputStream(nsIInputStream* aInputStream,
                     InputStreamParams& aParams);

void
SerializeInputStream(nsIInputStream* aInputStream,
                     OptionalInputStreamParams& aParams);

already_AddRefed<nsIInputStream>
DeserializeInputStream(const InputStreamParams& aParams);

already_AddRefed<nsIInputStream>
DeserializeInputStream(const OptionalInputStreamParams& aParams);

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_InputStreamUtils_h
