/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft= javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const l10n = require("gcli/l10n");
loader.lazyRequireGetter(this, "gDevTools",
                         "devtools/client/framework/devtools", true);

exports.items = [
  {
    item: "command",
    runAt: "client",
    name: "splitconsole",
    hidden: true,
    buttonId: "command-button-splitconsole",
    buttonClass: "command-button command-button-invertable",
    tooltipText: l10n.lookup("splitconsoleTooltip"),
    isRemoteSafe: true,
    state: {
      isChecked: function (target) {
        let toolbox = gDevTools.getToolbox(target);
        return !!(toolbox && toolbox.splitConsole);
      },
      onChange: function (target, changeHandler) {
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
    exec: function (args, context) {
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
    description: l10n.lookup("consoleDesc"),
    manual: l10n.lookup("consoleManual")
  },
  {
    item: "command",
    runAt: "client",
    name: "console clear",
    description: l10n.lookup("consoleclearDesc"),
    exec: function (args, context) {
      let toolbox = gDevTools.getToolbox(context.environment.target);
      if (toolbox == null) {
        return;
      }

      let panel = toolbox.getPanel("webconsole");
      if (panel == null) {
        return;
      }

      let onceMessagesCleared = panel.hud.jsterm.once("messages-cleared");
      panel.hud.jsterm.clearOutput();
      return onceMessagesCleared;
    }
  },
  {
    item: "command",
    runAt: "client",
    name: "console close",
    description: l10n.lookup("consolecloseDesc"),
    exec: function (args, context) {
      return gDevTools.closeToolbox(context.environment.target)
                      .then(() => {}); // Don't return a value to GCLI
    }
  },
  {
    item: "command",
    runAt: "client",
    name: "console open",
    description: l10n.lookup("consoleopenDesc"),
    exec: function (args, context) {
      const target = context.environment.target;
      return gDevTools.showToolbox(target, "webconsole")
                      .then(() => {}); // Don't return a value to GCLI
    }
  }
];
