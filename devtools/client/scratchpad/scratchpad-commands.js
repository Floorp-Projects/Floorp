/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const l10n = require("gcli/l10n");
const {Cu} = require("chrome");

exports.items = [{
  item: "command",
  runAt: "client",
  name: "scratchpad",
  buttonId: "command-button-scratchpad",
  buttonClass: "command-button command-button-invertable",
  tooltipText: l10n.lookup("scratchpadOpenTooltip"),
  hidden: true,
  exec: function (args, context) {
    const {ScratchpadManager} = Cu.import("resource://devtools/client/scratchpad/scratchpad-manager.jsm", {});
    ScratchpadManager.openScratchpad();
  }
}];
