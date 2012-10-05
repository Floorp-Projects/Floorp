/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_ProcessUtils_h
#define mozilla_ipc_ProcessUtils_h

namespace mozilla {
namespace ipc {

// You probably should call ContentChild::SetProcessName instead of calling
// this directly.
void SetThisProcessName(const char *aName);

} // namespace ipc
} // namespace mozilla

#endif // ifndef mozilla_ipc_ProcessUtils_h

