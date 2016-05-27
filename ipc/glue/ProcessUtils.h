/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_ProcessUtils_h
#define mozilla_ipc_ProcessUtils_h

#ifdef MOZ_B2G_LOADER
#include "base/process_util.h"
#endif

namespace mozilla {
namespace ipc {

// You probably should call ContentChild::SetProcessName instead of calling
// this directly.
void SetThisProcessName(const char *aName);

#ifdef MOZ_B2G_LOADER
// see ProcessUtils_linux.cpp for explaination.
void ProcLoaderClientGeckoInit();
bool ProcLoaderIsInitialized();
bool ProcLoaderLoad(const char *aArgv[],
                    const char *aEnvp[],
                    const base::file_handle_mapping_vector &aFdsRemap,
                    const base::ChildPrivileges aPrivs,
                    base::ProcessHandle *aProcessHandle);
#endif /* MOZ_B2G_LOADER */

} // namespace ipc
} // namespace mozilla

#endif // ifndef mozilla_ipc_ProcessUtils_h

