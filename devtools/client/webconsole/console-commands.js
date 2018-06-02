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
    buttonClass: "command-button",
    tooltipText: l10n.lookupFormat("splitconsoleTooltip2", ["Esc"]),
    isRemoteSafe: true,
    state: {
      isChecked: function(target) {
        const toolbox = gDevTools.getToolbox(target);
        return !!(toolbox && toolbox.splitConsole);
      },
      onChange: function(target, changeHandler) {
        // Register handlers for when a change event should be fired
        // (which resets the checked state of the button).
        const toolbox = gDevTools.getToolbox(target);
        if (!toolbox) {
          return;
        }

        const callback = changeHandler.bind(null, { target });
        toolbox.on("split-console", callback);
        toolbox.once("destroyed", () => {
          toolbox.off("split-console", callback);
        });
      }
    },
    exec: function(args, context) {
      const target = context.environment.target;
      const toolbox = gDevTools.getToolbox(target);

      if (!toolbox) {
        return gDevTools.showToolbox(target, "inspector").then((newToolbox) => {
          newToolbox.toggleSplitConsole();
        });
      }
      return toolbox.toggleSplitConsole();
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
    exec: function(args, context) {
      const toolbox = gDevTools.getToolbox(context.environment.target);
      if (toolbox == null) {
        return null;
      }

      const panel = toolbox.getPanel("webconsole");
      if (panel == null) {
        return null;
      }

      const onceMessagesCleared = panel.hud.once("messages-cleared");
      panel.hud.ui.clearOutput();
      return onceMessagesCleared;
    }
  },
  {
    item: "command",
    runAt: "client",
    name: "console close",
    description: l10n.lookup("consolecloseDesc"),
    exec: function(args, context) {
      // Don't return a value to GCLI
      return gDevTools.closeToolbox(context.environment.target).then(() => {});
    }
  },
  {
    item: "command",
    runAt: "client",
    name: "console open",
    description: l10n.lookup("consoleopenDesc"),
    exec: function(args, context) {
      const target = context.environment.target;
      // Don't return a value to GCLI
      return gDevTools.showToolbox(target, "webconsole").then(() => {});
    }
  }
];
