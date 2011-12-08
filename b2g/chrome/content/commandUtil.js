/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Mobile Browser.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Finkle <mfinkle@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


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

