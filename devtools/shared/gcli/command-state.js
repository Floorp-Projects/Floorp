/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/old-event-emitter");

const getTargetId = ({tab}) => tab.linkedBrowser.outerWindowID;
const enabledCommands = new Map();

/**
 * The `CommandState` is a singleton that provides utility methods to keep the commands'
 * state in sync between the toolbox, the toolbar and the content.
 */
const CommandState = EventEmitter.decorate({
  /**
   * Returns if a command is enabled on a given target.
   *
   * @param {Object} target
   *                  The target object must have a tab's reference.
   * @param {String} command
   *                  The command's name used in gcli.
   * @ returns {Boolean} returns `false` if the command is not enabled for the target
   *                    given, or if the target given hasn't a tab; `true` otherwise.
   */
  isEnabledForTarget(target, command) {
    if (!target.tab || !enabledCommands.has(command)) {
      return false;
    }

    return enabledCommands.get(command).has(getTargetId(target));
  },

  /**
   * Enables a command on a given target.
   * Emits a "changed" event to notify potential observers about the new commands state.
   *
   * @param {Object} target
   *                  The target object must have a tab's reference.
   * @param {String} command
   *                  The command's name used in gcli.
   */
  enableForTarget(target, command) {
    if (!target.tab) {
      return;
    }

    if (!enabledCommands.has(command)) {
      enabledCommands.set(command, new Set());
    }

    enabledCommands.get(command).add(getTargetId(target));

    CommandState.emit("changed", {target, command});
  },

  /**
   * Disabled a command on a given target.
   * Emits a "changed" event to notify potential observers about the new commands state.
   *
   * @param {Object} target
   *                  The target object must have a tab's reference.
   * @param {String} command
   *                  The command's name used in gcli.
   */
  disableForTarget(target, command) {
    if (!target.tab || !enabledCommands.has(command)) {
      return;
    }

    enabledCommands.get(command).delete(getTargetId(target));

    CommandState.emit("changed", {target, command});
  },
});
exports.CommandState = CommandState;

