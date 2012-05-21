/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentChild.h"
#include "nsHapticFeedback.h"
#include "AndroidBridge.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS1(nsHapticFeedback, nsIHapticFeedback)

NS_IMETHODIMP
nsHapticFeedback::PerformSimpleAction(PRInt32 aType)
{
    AndroidBridge* bridge = AndroidBridge::Bridge();
    if (bridge) {
        bridge->PerformHapticFeedback(aType == LongPress);
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}
