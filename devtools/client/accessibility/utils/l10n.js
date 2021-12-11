/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { LocalizationHelper } = require("devtools/shared/l10n");

const A11Y_STRINGS_URI = "devtools/client/locales/accessibility.properties";

exports.L10N = new LocalizationHelper(A11Y_STRINGS_URI);
