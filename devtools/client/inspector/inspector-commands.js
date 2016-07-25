/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const l10n = require("gcli/l10n");
loader.lazyRequireGetter(this, "gDevTools", "devtools/client/framework/devtools", true);
const {EyeDropper, HighlighterEnvironment} = require("devtools/server/actors/highlighters");
const Telemetry = require("devtools/client/shared/telemetry");

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
  exec: function (args, context) {
    let target = context.environment.target;
    return gDevTools.showToolbox(target, "inspector").then(toolbox => {
      toolbox.getCurrentPanel().selection.setNode(args.selector, "gcli");
    });
  }
}, {
  item: "command",
  runAt: "client",
  name: "eyedropper",
  description: l10n.lookup("eyedropperDesc"),
  manual: l10n.lookup("eyedropperManual"),
  params: [{
    // This hidden parameter is only set to true when the eyedropper browser menu item is
    // used. It is useful to log a different telemetry event whether the tool was used
    // from the menu, or from the gcli command line.
    group: "hiddengroup",
    params: [{
      name: "frommenu",
      type: "boolean",
      hidden: true
    }]
  }],
  exec: function (args, context) {
    let telemetry = new Telemetry();
    telemetry.toolOpened(args.frommenu ? "menueyedropper" : "eyedropper");
    context.updateExec("eyedropper_server").catch(e => console.error(e));
  }
}, {
  item: "command",
  runAt: "server",
  name: "eyedropper_server",
  hidden: true,
  exec: function (args, {environment}) {
    let env = new HighlighterEnvironment();
    env.initFromWindow(environment.window);
    let eyeDropper = new EyeDropper(env);

    eyeDropper.show(environment.document.documentElement, {copyOnSelect: true});

    eyeDropper.once("hidden", () => {
      eyeDropper.destroy();
      env.destroy();
    });
  }
}];
