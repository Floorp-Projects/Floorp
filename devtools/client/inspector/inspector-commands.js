/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const l10n = require("gcli/l10n");
const {gDevTools} = require("devtools/client/framework/devtools");
/* eslint-disable mozilla/reject-some-requires */
const {HighlighterEnvironment} = require("devtools/server/actors/highlighters");
const {EyeDropper} = require("devtools/server/actors/highlighters/eye-dropper");
/* eslint-enable mozilla/reject-some-requires */
const Telemetry = require("devtools/client/shared/telemetry");

const TELEMETRY_EYEDROPPER_OPENED = "DEVTOOLS_EYEDROPPER_OPENED_COUNT";
const TELEMETRY_EYEDROPPER_OPENED_MENU = "DEVTOOLS_MENU_EYEDROPPER_OPENED_COUNT";

const windowEyeDroppers = new WeakMap();

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
  exec: async function(args, context) {
    let target = context.environment.target;
    let toolbox = await gDevTools.showToolbox(target, "inspector");
    let walker = toolbox.getCurrentPanel().walker;
    let rootNode = await walker.getRootNode();
    let nodeFront = await walker.querySelector(rootNode, args.selector);
    toolbox.getCurrentPanel().selection.setNodeFront(nodeFront, { reason: "gcli" });
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
    }, {
      name: "hide",
      type: "boolean",
      hidden: true
    }]
  }],
  exec: async function(args, context) {
    if (args.hide) {
      context.updateExec("eyedropper_server_hide").catch(console.error);
      return;
    }

    // If the inspector is already picking a color from the page, cancel it.
    let target = context.environment.target;
    let toolbox = gDevTools.getToolbox(target);
    if (toolbox) {
      let inspector = toolbox.getPanel("inspector");
      if (inspector) {
        await inspector.hideEyeDropper();
      }
    }

    let telemetry = new Telemetry();
    if (args.frommenu) {
      telemetry.getHistogramById(TELEMETRY_EYEDROPPER_OPENED_MENU).add(true);
    } else {
      telemetry.getHistogramById(TELEMETRY_EYEDROPPER_OPENED).add(true);
    }
    context.updateExec("eyedropper_server").catch(console.error);
  }
}, {
  item: "command",
  runAt: "server",
  name: "eyedropper_server",
  hidden: true,
  exec: function(args, {environment}) {
    let eyeDropper = windowEyeDroppers.get(environment.window);

    if (!eyeDropper) {
      let env = new HighlighterEnvironment();
      env.initFromWindow(environment.window);

      eyeDropper = new EyeDropper(env);
      eyeDropper.once("hidden", () => {
        eyeDropper.destroy();
        env.destroy();
        windowEyeDroppers.delete(environment.window);
      });

      windowEyeDroppers.set(environment.window, eyeDropper);
    }

    eyeDropper.show(environment.document.documentElement, {copyOnSelect: true});
  }
}, {
  item: "command",
  runAt: "server",
  name: "eyedropper_server_hide",
  hidden: true,
  exec: function(args, {environment}) {
    let eyeDropper = windowEyeDroppers.get(environment.window);
    if (eyeDropper) {
      eyeDropper.hide();
    }
  }
}];
