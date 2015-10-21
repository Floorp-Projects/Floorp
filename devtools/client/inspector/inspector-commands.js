/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const l10n = require("gcli/l10n");

exports.items = [{
  item: "command",
  runAt: "server",
  name: "inspect",
  description: l10n.lookup("inspectDesc"),
  manual: l10n.lookup("inspectManual"),
  params: [
    {
      name: "selector",
      type: "node",
      description: l10n.lookup("inspectNodeDesc"),
      manual: l10n.lookup("inspectNodeManual")
    }
  ],
  exec: function(args, context) {
    let target = context.environment.target;
    let gDevTools = require("resource://devtools/client/framework/gDevTools.jsm").gDevTools;

    return gDevTools.showToolbox(target, "inspector").then(toolbox => {
      toolbox.getCurrentPanel().selection.setNode(args.selector, "gcli");
    });
  }
}];
