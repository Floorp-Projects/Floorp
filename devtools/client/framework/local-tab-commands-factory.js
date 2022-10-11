/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyRequireGetter(
  this,
  "CommandsFactory",
  "resource://devtools/shared/commands/commands-factory.js",
  true
);

// Map of existing Commands objects, keyed by XULTab.
const commandsMap = new WeakMap();

/**
 * Functions for creating unique Commands for Local Tabs.
 */
exports.LocalTabCommandsFactory = {
  /**
   * Create a unique commands object for the given tab.
   *
   * If a commands was already created by this factory for the provided tab,
   * it will be returned and no new commands created.
   *
   * Otherwise, this will automatically:
   * - spawn a DevToolsServer in the parent process,
   * - create a DevToolsClient
   * - connect the DevToolsClient to the DevToolsServer
   * - call RootActor's `getTab` request to retrieve the WindowGlobalTargetActor's form
   *
   * @param {XULTab} tab
   *        The tab to use in creating a new commands.
   *
   * @return {Commands object} The commands object for the provided tab.
   */
  async createCommandsForTab(tab) {
    let commands = commandsMap.get(tab);
    if (commands) {
      // Keep in mind that commands can be either a promise
      // or a commands object.
      return commands;
    }

    const promise = CommandsFactory.forTab(tab);
    // Immediately set the commands's promise in cache to prevent race
    commandsMap.set(tab, promise);
    commands = await promise;
    // Then replace the promise with the commands object
    commandsMap.set(tab, commands);

    commands.descriptorFront.once("descriptor-destroyed", () => {
      commandsMap.delete(tab);
    });
    return commands;
  },

  /**
   * Retrieve an existing commands created by this factory for the provided
   * tab. Returns null if no commands was created yet.
   *
   * @param {XULTab} tab
   *        The tab for which the commands should be retrieved
   */
  async getCommandsForTab(tab) {
    // commandsMap.get(tab) can either return an initialized commands, a promise
    // which will resolve a commands, or null if no commands was ever created
    // for this tab.
    return commandsMap.get(tab);
  },
};
