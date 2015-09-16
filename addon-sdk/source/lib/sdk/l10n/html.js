/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "unstable"
};

const { processes, remoteRequire } = require("../remote/parent");
remoteRequire("sdk/content/l10n-html");

var enabled = false;
function enable() {
  if (!enabled) {
    processes.port.emit("sdk/l10n/html/enable");
    enabled = true;
  }
}
exports.enable = enable;

function disable() {
  if (enabled) {
    processes.port.emit("sdk/l10n/html/disable");
    enabled = false;
  }
}
exports.disable = disable;

processes.forEvery(process => {
  process.port.emit(enabled ? "sdk/l10n/html/enable" : "sdk/l10n/html/disable");
});
