/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const gcli = require("gcli/index");
const { gDevTools } = require("resource:///modules/devtools/gDevTools.jsm");

exports.items = [
  {
    name: 'splitconsole',
    hidden: true,
    buttonId: "command-button-splitconsole",
    buttonClass: "command-button command-button-invertable",
    tooltipText: gcli.lookup("splitconsoleTooltip"),
    state: {
      isChecked: function(target) {
        let toolbox = gDevTools.getToolbox(target);
        return !!(toolbox && toolbox.splitConsole);
      },
      onChange: function(target, changeHandler) {
        // Register handlers for when a change event should be fired
        // (which resets the checked state of the button).
        let toolbox = gDevTools.getToolbox(target);
        let callback = changeHandler.bind(null, "changed", { target: target });

        if (!toolbox) {
          return;
        }

        toolbox.on("split-console", callback);
        toolbox.once("destroyed", () => {
          toolbox.off("split-console", callback);
        });
      }
    },
    exec: function(args, context) {
      let target = context.environment.target;
      let toolbox = gDevTools.getToolbox(target);

      if (!toolbox) {
        return gDevTools.showToolbox(target, "inspector").then((toolbox) => {
          toolbox.toggleSplitConsole();
        });
      } else {
        toolbox.toggleSplitConsole();
      }
    }
  },
  {
    name: "console",
    description: gcli.lookup("consoleDesc"),
    manual: gcli.lookup("consoleManual")
  },
  {
    name: "console clear",
    description: gcli.lookup("consoleclearDesc"),
    exec: function(args, context) {
      let toolbox = gDevTools.getToolbox(context.environment.target);
      if (toolbox == null) {
        return;
      }

      let panel = toolbox.getPanel("webconsole");
      if (panel == null) {
        return;
      }

      panel.hud.jsterm.clearOutput();
    }
  },
  {
    name: "console close",
    description: gcli.lookup("consolecloseDesc"),
    exec: function(args, context) {
      return gDevTools.closeToolbox(context.environment.target);
    }
  },
  {
    name: "console open",
    description: gcli.lookup("consoleopenDesc"),
    exec: function(args, context) {
      return gDevTools.showToolbox(context.environment.target, "webconsole");
    }
  }
];
