/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ipc_glue_LibrarySandboxPreload_h
#define ipc_glue_LibrarySandboxPreload_h

#include "nsString.h"

namespace mozilla {
namespace ipc {
nsCString GetSandboxedRLBoxPath();
void PreloadSandboxedDynamicLibrary();
}  // namespace ipc
}  // namespace mozilla
#endif
