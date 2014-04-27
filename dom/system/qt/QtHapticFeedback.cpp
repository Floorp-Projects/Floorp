/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <QFeedbackEffect>
#include "QtHapticFeedback.h"

NS_IMPL_ISUPPORTS(QtHapticFeedback, nsIHapticFeedback)

NS_IMETHODIMP
QtHapticFeedback::PerformSimpleAction(int32_t aType)
{
    if (aType == ShortPress)
        QFeedbackEffect::playThemeEffect(QFeedbackEffect::PressWeak);
    if (aType == LongPress)
        QFeedbackEffect::playThemeEffect(QFeedbackEffect::PressStrong);

    return NS_OK;
}
