/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const gcli = require("gcli/index");

exports.items = [{
  name: "scratchpad",
  buttonId: "command-button-scratchpad",
  buttonClass: "command-button command-button-invertable",
  tooltipText: gcli.lookup("scratchpadOpenTooltip"),
  hidden: true,
  exec: function(args, context) {
    let Scratchpad = context.environment.chromeWindow.Scratchpad;
    Scratchpad.ScratchpadManager.openScratchpad();
  }
}];
