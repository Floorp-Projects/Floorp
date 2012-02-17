/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- /
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Command Updater
 */
let CommandUpdater = {
  /**
   * Gets a controller that can handle a particular command.
   * @param {string} command
   *        A command to locate a controller for, preferring controllers that
   *        show the command as enabled.
   * @return {object} In this order of precedence:
   *            - the first controller supporting the specified command
   *              associated with the focused element that advertises the
   *              command as ENABLED.
   *            - the first controller supporting the specified command
   *              associated with the global window that advertises the
   *              command as ENABLED.
   *            - the first controller supporting the specified command
   *              associated with the focused element.
   *            - the first controller supporting the specified command
   *              associated with the global window.
   */
  _getControllerForCommand: function(command) {
    try {
      let commandDispatcher = top.document.commandDispatcher;
      let controller = commandDispatcher.getControllerForCommand(command);
      if (controller && controller.isCommandEnabled(command))
        return controller;
    }
    catch (e) { }

    let controllerCount = window.controllers.getControllerCount();
    for (let i = 0; i < controllerCount; ++i) {
      let current = window.controllers.getControllerAt(i);
      try {
        if (current.supportsCommand(command) &&
            current.isCommandEnabled(command))
          return current;
      }
      catch (e) { }
    }
    return controller || window.controllers.getControllerForCommand(command);
  },

  /**
   * Updates the state of a XUL <command> element for the specified command
   * depending on its state.
   * @param {string} command
   *        The name of the command to update the XUL <command> element for.
   */
  updateCommand: function(command) {
    let enabled = false;
    try {
      let controller = this._getControllerForCommand(command);
      if (controller) {
        enabled = controller.isCommandEnabled(command);
      }
    }
    catch (ex) { }

    this.enableCommand(command, enabled);
  },

  /**
   * Updates the state of a XUL <command> element for the specified command
   * depending on its state.
   * @param {string} command
   *        The name of the command to update the XUL <command> element for.
   */
  updateCommands: function(_commands) {
    let commands = _commands.split(',');
    for (let command in commands) {
      this.updateCommand(commands[command]);
    }
  },

  /**
   * Enables or disables a XUL <command> element.
   * @param {string} command
   *          The name of the command to enable or disable.
   * @param {bool} enabled
   *          true if the command should be enabled, false otherwise.
   */
  enableCommand: function(command, enabled) {
    let element = document.getElementById(command);
    if (!element)
      return;

    if (enabled)
      element.removeAttribute('disabled');
    else
      element.setAttribute('disabled', 'true');
  },

  /**
   * Performs the action associated with a specified command using the most
   * relevant controller.
   * @param {string} command
   *          The command to perform.
   */
  doCommand: function(command) {
    let controller = this._getControllerForCommand(command);
    if (!controller)
      return;
    controller.doCommand(command);
  },

  /**
   * Changes the label attribute for the specified command.
   * @param {string} command
   *          The command to update.
   * @param {string} labelAttribute
   *          The label value to use.
   */
  setMenuValue: function(command, labelAttribute) {
    let commandNode = top.document.getElementById(command);
    if (commandNode) {
      let label = commandNode.getAttribute(labelAttribute);
      if (label)
        commandNode.setAttribute('label', label);
    }
  },

  /**
   * Changes the accesskey attribute for the specified command.
   * @param {string} command
   *          The command to update.
   * @param {string} valueAttribute
   *          The value attribute to use.
   */
  setAccessKey: function(command, valueAttribute) {
    let commandNode = top.document.getElementById(command);
    if (commandNode) {
      let value = commandNode.getAttribute(valueAttribute);
      if (value)
        commandNode.setAttribute('accesskey', value);
    }
  },

  /**
   * Inform all the controllers attached to a node that an event has occurred
   * (e.g. the tree controllers need to be informed of blur events so that they
   * can change some of the menu items back to their default values)
   * @param  {node} node
   *          The node receiving the event.
   * @param  {event} event
   *          The event.
   */
  onEvent: function(node, event) {
    let numControllers = node.controllers.getControllerCount();
    let controller;

    for (let i = 0; i < numControllers; i++) {
      controller = node.controllers.getControllerAt(i);
      if (controller)
        controller.onEvent(event);
    }
  }
};

