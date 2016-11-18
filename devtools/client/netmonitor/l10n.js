/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {LocalizationHelper} = require("devtools/shared/l10n");

const NET_STRINGS_URI = "devtools/client/locales/netmonitor.properties";
const WEBCONSOLE_STRINGS_URI = "devtools/client/locales/webconsole.properties";

exports.L10N = new LocalizationHelper(NET_STRINGS_URI);
exports.WEBCONSOLE_L10N = new LocalizationHelper(WEBCONSOLE_STRINGS_URI);
