/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = [ ];

Components.utils.import("resource://gre/modules/devtools/gcli.jsm");

/**
 * 'scratchpad' command
 */
gcli.addCommand({
  name: "scratchpad",
  buttonId: "command-button-scratchpad",
  buttonClass: "command-button",
  tooltipText: gcli.lookup("scratchpadOpenTooltip"),
  hidden: true,
  exec: function(args, context) {
    let chromeWindow = context.environment.chromeDocument.defaultView;
    chromeWindow.Scratchpad.ScratchpadManager.openScratchpad();
  }
});
