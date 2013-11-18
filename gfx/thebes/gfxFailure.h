/* vim: se cin sw=2 ts=2 et : */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gfxFailure_h_
#define gfxFailure_h_

#include "nsString.h"
#include "nsIGfxInfo.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {
    namespace gfx {
        inline
        void LogFailure(const nsCString &failure) {
            nsCOMPtr<nsIGfxInfo> gfxInfo = do_GetService("@mozilla.org/gfx/info;1");
            gfxInfo->LogFailure(failure);
        }
    }
}

#endif // gfxFailure_h_
