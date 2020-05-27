//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDocShellTelemetryUtils_h__
#define nsDocShellTelemetryUtils_h__

#include "mozilla/Telemetry.h"

namespace mozilla {
namespace dom {
/**
 * Convert page load errors to telemetry labels
 * Only select nsresults are converted, otherwise this function
 * will return "errorOther", view the list of errors at
 * docshell/base/nsDocShellTelemetryUtils.cpp.
 */
Telemetry::LABELS_PAGE_LOAD_ERROR LoadErrorToTelemetryLabel(nsresult aRv);
}  // namespace dom
}  // namespace mozilla
#endif  // nsDocShellTelemetryUtils_h__
