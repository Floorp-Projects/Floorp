"use strict";

const {LocalizationHelper} = require("devtools/shared/l10n");

const NET_STRINGS_URI = "devtools/locale/netmonitor.properties";
const WEBCONSOLE_STRINGS_URI = "devtools/locale/webconsole.properties";

exports.L10N = new LocalizationHelper(NET_STRINGS_URI);
exports.WEBCONSOLE_L10N = new LocalizationHelper(WEBCONSOLE_STRINGS_URI);
