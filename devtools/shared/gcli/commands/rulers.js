/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* globals getBrowserForTab */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const eventEmitter = new EventEmitter();
const events = require("sdk/event/core");
loader.lazyRequireGetter(this, "getOuterId", "sdk/window/utils", true);
loader.lazyRequireGetter(this, "getBrowserForTab", "sdk/tabs/utils", true);

const l10n = require("gcli/l10n");
require("devtools/server/actors/inspector");
const { RulersHighlighter, HighlighterEnvironment } =
  require("devtools/server/actors/highlighters");

const highlighters = new WeakMap();
const visibleHighlighters = new Set();

const isCheckedFor = (tab) =>
  tab ? visibleHighlighters.has(getBrowserForTab(tab).outerWindowID) : false;

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
      isChecked: ({_tab}) => isCheckedFor(_tab),
      onChange: (target, handler) => eventEmitter.on("changed", handler),
      offChange: (target, handler) => eventEmitter.off("changed", handler)
    },
    exec: function*(args, context) {
      let { target } = context.environment;

      // Pipe the call to the server command.
      let response = yield context.updateExec("rulers_server");
      let { visible, id } = response.data;

      if (visible) {
        visibleHighlighters.add(id);
      } else {
        visibleHighlighters.delete(id);
      }

      eventEmitter.emit("changed", { target });

      // Toggle off the button when the page navigates because the rulers are
      // removed automatically by the RulersHighlighter on the server then.
      let onNavigate = () => {
        visibleHighlighters.delete(id);
        eventEmitter.emit("changed", { target });
      };
      target.off("will-navigate", onNavigate);
      target.once("will-navigate", onNavigate);
    }
  },
  // The server rulers command is hidden by default, it's just used by the
  // client command.
  {
    name: "rulers_server",
    runAt: "server",
    hidden: true,
    returnType: "highlighterVisibility",
    exec: function(args, context) {
      let env = context.environment;
      let { document } = env;
      let id = getOuterId(env.window);

      // Calling the command again after the rulers have been shown once hides
      // them.
      if (highlighters.has(document)) {
        let { highlighter } = highlighters.get(document);
        highlighter.destroy();
        return {visible: false, id};
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
      events.once(highlighter, "destroy", () => {
        if (highlighters.has(document)) {
          let { environment } = highlighters.get(document);
          environment.destroy();
          highlighters.delete(document);
        }
      });

      highlighter.show();
      return {visible: true, id};
    }
  }
];
