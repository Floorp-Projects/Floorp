/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProcessUtils.h"

#include <pthread.h>

#if !defined(OS_NETBSD)
#include <pthread_np.h>
#endif

namespace mozilla {
namespace ipc {

void SetThisProcessName(const char *aName)
{
#if defined(OS_NETBSD)
  pthread_setname_np(pthread_self(), "%s", (void *)aName);
#else
  pthread_set_name_np(pthread_self(), aName);
#endif
}

} // namespace ipc
} // namespace mozilla
