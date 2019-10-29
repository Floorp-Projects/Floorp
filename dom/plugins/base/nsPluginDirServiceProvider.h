/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPluginDirServiceProvider_h_
#define nsPluginDirServiceProvider_h_

#ifdef XP_WIN
#  include "nsCOMPtr.h"
#  include "nsTArray.h"

static nsresult GetPLIDDirectories(nsTArray<nsCOMPtr<nsIFile>>& aDirs);

static nsresult GetPLIDDirectoriesWithRootKey(
    uint32_t aKey, nsTArray<nsCOMPtr<nsIFile>>& aDirs);

#endif  // XP_WIN
#endif  // nsPluginDirServiceProvider_h_
