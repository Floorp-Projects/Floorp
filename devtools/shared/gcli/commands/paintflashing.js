/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");

loader.lazyRequireGetter(this, "CommandState",
  "devtools/shared/gcli/command-state", true);

var telemetry;
try {
  const Telemetry = require("devtools/client/shared/telemetry");
  telemetry = new Telemetry();
} catch (e) {
  // DevTools Telemetry module only available in Firefox
}

const gcli = require("gcli/index");
const l10n = require("gcli/l10n");

/**
 * Fire events and telemetry when paintFlashing happens
 */
function onPaintFlashingChanged(target, flashing) {
  if (flashing) {
    CommandState.enableForTarget(target, "paintflashing");
  } else {
    CommandState.disableForTarget(target, "paintflashing");
  }

  target.once("will-navigate", () =>
    CommandState.disableForTarget(target, "paintflashing"));

  if (!telemetry) {
    return;
  }
  if (flashing) {
    telemetry.toolOpened("paintflashing");
  } else {
    telemetry.toolClosed("paintflashing");
  }
}

/**
 * Alter the paintFlashing state of a window and report on the new value.
 * This works with chrome or content windows.
 *
 * This is a bizarre method that you could argue should be broken up into
 * separate getter and setter functions, however keeping it as one helps
 * to simplify the commands below.
 *
 * @param state {string} One of:
 * - "on" which does window.paintFlashing = true
 * - "off" which does window.paintFlashing = false
 * - "toggle" which does window.paintFlashing = !window.paintFlashing
 * - "query" which does nothing
 * @return The new value of the window.paintFlashing flag
 */
function setPaintFlashing(window, state) {
  const winUtils = window.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIDOMWindowUtils);

  if (!["on", "off", "toggle", "query"].includes(state)) {
    throw new Error(`Unsupported state: ${state}`);
  }

  if (state === "on") {
    winUtils.paintFlashing = true;
  } else if (state === "off") {
    winUtils.paintFlashing = false;
  } else if (state === "toggle") {
    winUtils.paintFlashing = !winUtils.paintFlashing;
  }

  return winUtils.paintFlashing;
}

exports.items = [
  {
    name: "paintflashing",
    description: l10n.lookup("paintflashingDesc")
  },
  {
    item: "command",
    runAt: "client",
    name: "paintflashing on",
    description: l10n.lookup("paintflashingOnDesc"),
    manual: l10n.lookup("paintflashingManual"),
    params: [{
      group: "options",
      params: [
        {
          type: "boolean",
          name: "chrome",
          get hidden() {
            return gcli.hiddenByChromePref();
          },
          description: l10n.lookup("paintflashingChromeDesc"),
        }
      ]
    }],
    exec: function* (args, context) {
      if (!args.chrome) {
        const output = yield context.updateExec("paintflashing_server --state on");

        onPaintFlashingChanged(context.environment.target, output.data);
      } else {
        setPaintFlashing(context.environment.chromeWindow, "on");
      }
    }
  },
  {
    item: "command",
    runAt: "client",
    name: "paintflashing off",
    description: l10n.lookup("paintflashingOffDesc"),
    manual: l10n.lookup("paintflashingManual"),
    params: [{
      group: "options",
      params: [
        {
          type: "boolean",
          name: "chrome",
          get hidden() {
            return gcli.hiddenByChromePref();
          },
          description: l10n.lookup("paintflashingChromeDesc"),
        }
      ]
    }],
    exec: function* (args, context) {
      if (!args.chrome) {
        const output = yield context.updateExec("paintflashing_server --state off");

        onPaintFlashingChanged(context.environment.target, output.data);
      } else {
        setPaintFlashing(context.environment.chromeWindow, "off");
      }
    }
  },
  {
    item: "command",
    runAt: "client",
    name: "paintflashing toggle",
    hidden: true,
    buttonId: "command-button-paintflashing",
    buttonClass: "command-button",
    state: {
      isChecked: (target) => CommandState.isEnabledForTarget(target, "paintflashing"),
      onChange: (_, handler) => CommandState.on("changed", handler),
      offChange: (_, handler) => CommandState.off("changed", handler),
    },
    tooltipText: l10n.lookup("paintflashingTooltip"),
    description: l10n.lookup("paintflashingToggleDesc"),
    manual: l10n.lookup("paintflashingManual"),
    exec: function* (args, context) {
      const output = yield context.updateExec("paintflashing_server --state toggle");

      onPaintFlashingChanged(context.environment.target, output.data);
    }
  },
  {
    item: "command",
    runAt: "server",
    name: "paintflashing_server",
    hidden: true,
    params: [
      {
        name: "state",
        type: {
          name: "selection",
          data: [ "on", "off", "toggle", "query" ]
        }
      },
    ],
    returnType: "paintFlashingState",
    exec: function(args, context) {
      const { window } = context.environment;

      return setPaintFlashing(window, args.state);
    }
  }
];
