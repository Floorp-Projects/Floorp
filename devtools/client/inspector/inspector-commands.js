/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const l10n = require("gcli/l10n");
const {gDevTools} = require("devtools/client/framework/devtools");
/* eslint-disable mozilla/reject-some-requires */
const {EyeDropper, HighlighterEnvironment} = require("devtools/server/actors/highlighters");
/* eslint-enable mozilla/reject-some-requires */
const Telemetry = require("devtools/client/shared/telemetry");

exports.items = [{
  item: "command",
  runAt: "client",
  name: "inspect",
  description: l10n.lookup("inspectDesc"),
  manual: l10n.lookup("inspectManual"),
  params: [
    {
      name: "selector",
      type: "string",
      description: l10n.lookup("inspectNodeDesc"),
      manual: l10n.lookup("inspectNodeManual")
    }
  ],
  exec: function* (args, context) {
    let target = context.environment.target;
    let toolbox = yield gDevTools.showToolbox(target, "inspector");
    let walker = toolbox.getCurrentPanel().walker;
    let rootNode = yield walker.getRootNode();
    let nodeFront = yield walker.querySelector(rootNode, args.selector);
    toolbox.getCurrentPanel().selection.setNodeFront(nodeFront, "gcli");
  },
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
