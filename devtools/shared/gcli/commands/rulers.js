/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");

loader.lazyRequireGetter(this, "CommandState",
  "devtools/shared/gcli/command-state", true);

const l10n = require("gcli/l10n");
require("devtools/server/actors/inspector");
const { RulersHighlighter, HighlighterEnvironment } =
  require("devtools/server/actors/highlighters");

const highlighters = new WeakMap();

exports.items = [
  // The client rulers command is used to maintain the toolbar button state only
  // and redirects to the server command to actually toggle the rulers (see
  // rulers_server below).
  {
    name: "rulers",
    runAt: "client",
    description: l10n.lookup("rulersDesc"),
    manual: l10n.lookup("rulersManual"),
    buttonId: "command-button-rulers",
    buttonClass: "command-button command-button-invertable",
    tooltipText: l10n.lookup("rulersTooltip"),
    state: {
      isChecked: (target) => CommandState.isEnabledForTarget(target, "rulers"),
      onChange: (target, handler) => CommandState.on("changed", handler),
      offChange: (target, handler) => CommandState.off("changed", handler)
    },
    exec: function* (args, context) {
      let { target } = context.environment;

      // Pipe the call to the server command.
      let response = yield context.updateExec("rulers_server");
      let isEnabled = response.data;

      if (isEnabled) {
        CommandState.enableForTarget(target, "rulers");
      } else {
        CommandState.disableForTarget(target, "rulers");
      }

      // Toggle off the button when the page navigates because the rulers are
      // removed automatically by the RulersHighlighter on the server then.
      target.once("will-navigate", () => CommandState.disableForTarget(target, "rulers"));
    }
  },
  // The server rulers command is hidden by default, it's just used by the
  // client command.
  {
    name: "rulers_server",
    runAt: "server",
    hidden: true,
    returnType: "highlighterVisibility",
    exec: function (args, context) {
      let env = context.environment;
      let { document } = env;

      // Calling the command again after the rulers have been shown once hides
      // them.
      if (highlighters.has(document)) {
        let { highlighter } = highlighters.get(document);
        highlighter.destroy();
        return false;
      }

      // Otherwise, display the rulers.
      let environment = new HighlighterEnvironment();
      environment.initFromWindow(env.window);
      let highlighter = new RulersHighlighter(environment);

      // Store the instance of the rulers highlighter for this document so we
      // can hide it later.
      highlighters.set(document, { highlighter, environment });

      // Listen to the highlighter's destroy event which may happen if the
      // window is refreshed or closed with the rulers shown.
      EventEmitter.once(highlighter, "destroy", () => {
        if (highlighters.has(document)) {
          let { environment: toDestroy } = highlighters.get(document);
          toDestroy.destroy();
          highlighters.delete(document);
        }
      });

      highlighter.show();
      return true;
    }
  }
];
