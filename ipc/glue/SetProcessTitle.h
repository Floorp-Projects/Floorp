/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_SetProcessTitle_h
#define mozilla_ipc_SetProcessTitle_h

#include <string>
#include <vector>

namespace mozilla {

void SetProcessTitle(const std::vector<std::string>& aNewArgv);
void SetProcessTitleInit(char** aOrigArgv);

}  // namespace mozilla

#endif  // mozilla_ipc_SetProcessTitle_h
