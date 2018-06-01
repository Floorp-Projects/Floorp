/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");

loader.lazyRequireGetter(this, "CommandState",
  "devtools/shared/gcli/command-state", true);

const l10n = require("gcli/l10n");
require("devtools/server/actors/inspector/inspector");
const { HighlighterEnvironment } =
  require("devtools/server/actors/highlighters");
const { MeasuringToolHighlighter } =
  require("devtools/server/actors/highlighters/measuring-tool");

const highlighters = new WeakMap();

exports.items = [
  // The client measure command is used to maintain the toolbar button state
  // only and redirects to the server command to actually toggle the measuring
  // tool (see `measure_server` below).
  {
    name: "measure",
    runAt: "client",
    description: l10n.lookup("measureDesc"),
    manual: l10n.lookup("measureManual"),
    buttonId: "command-button-measure",
    buttonClass: "command-button",
    tooltipText: l10n.lookup("measureTooltip"),
    state: {
      isChecked: (target) => CommandState.isEnabledForTarget(target, "measure"),
      onChange: (target, handler) => CommandState.on("changed", handler),
      offChange: (target, handler) => CommandState.off("changed", handler)
    },
    exec: function* (args, context) {
      const { target } = context.environment;

      // Pipe the call to the server command.
      const response = yield context.updateExec("measure_server");
      const isEnabled = response.data;

      if (isEnabled) {
        CommandState.enableForTarget(target, "measure");
      } else {
        CommandState.disableForTarget(target, "measure");
      }

      // Toggle off the button when the page navigates because the measuring
      // tool is removed automatically by the MeasuringToolHighlighter on the
      // server then.
      target.once("will-navigate", () =>
        CommandState.disableForTarget(target, "measure"));
    }
  },
  // The server measure command is hidden by default, it's just used by the
  // client command.
  {
    name: "measure_server",
    runAt: "server",
    hidden: true,
    returnType: "highlighterVisibility",
    exec: function(args, context) {
      const env = context.environment;
      const { document } = env;

      // Calling the command again after the measuring tool has been shown once,
      // hides it.
      if (highlighters.has(document)) {
        const { highlighter } = highlighters.get(document);
        highlighter.destroy();
        return false;
      }

      // Otherwise, display the measuring tool.
      const environment = new HighlighterEnvironment();
      environment.initFromWindow(env.window);
      const highlighter = new MeasuringToolHighlighter(environment);

      // Store the instance of the measuring tool highlighter for this document
      // so we can hide it later.
      highlighters.set(document, { highlighter, environment });

      // Listen to the highlighter's destroy event which may happen if the
      // window is refreshed or closed with the measuring tool shown.
      EventEmitter.once(highlighter, "destroy", () => {
        if (highlighters.has(document)) {
          const { environment: toDestroy } = highlighters.get(document);
          toDestroy.destroy();
          highlighters.delete(document);
        }
      });

      highlighter.show();
      return true;
    }
  }
];
