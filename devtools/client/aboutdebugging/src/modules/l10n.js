/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FluentL10n,
} = require("devtools/client/shared/fluent-l10n/fluent-l10n");

// exports a singleton, which will be used across all aboutdebugging modules
exports.l10n = new FluentL10n();
