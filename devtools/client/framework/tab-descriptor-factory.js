/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyRequireGetter(
  this,
  "CommandsFactory",
  "devtools/shared/commands/commands-factory",
  true
);

// Map of existing Commands objects, keyed by XULTab.
const commandsMap = new WeakMap();

/**
 * Functions for creating (local) Tab Target Descriptors
 */
exports.TabDescriptorFactory = {
  /**
   * Create a unique tab target descriptor for the given tab.
   *
   * If a descriptor was already created by this factory for the provided tab,
   * it will be returned and no new descriptor created.
   *
   * Otherwise, this will automatically:
   * - spawn a DevToolsServer in the parent process,
   * - create a DevToolsClient
   * - connect the DevToolsClient to the DevToolsServer
   * - call RootActor's `getTab` request to retrieve the WindowGlobalTargetActor's form
   *
   * @param {XULTab} tab
   *        The tab to use in creating a new descriptor.
   *
   * @return {TabDescriptorFront} The tab descriptor for the provided tab.
   */
  async createDescriptorForTab(tab) {
    let commands = commandsMap.get(tab);
    if (commands) {
      commands = await commands;
      return commands.descriptorFront;
    }

    const promise = CommandsFactory.forTab(tab);
    // Immediately set the descriptor's promise in cache to prevent race
    commandsMap.set(tab, promise);
    commands = await promise;
    // Then replace the promise with the descriptor object
    commandsMap.set(tab, commands);

    commands.descriptorFront.once("descriptor-destroyed", () => {
      commandsMap.delete(tab);
    });
    return commands.descriptorFront;
  },

  /**
   * Retrieve an existing descriptor created by this factory for the provided
   * tab. Returns null if no descriptor was created yet.
   *
   * @param {XULTab} tab
   *        The tab for which the descriptor should be retrieved
   */
  async getDescriptorForTab(tab) {
    // commandsMap.get(tab) can either return initialized commands, a promise
    // which will resolve a commands, or null if no commands was ever created
    // for this tab.
    const commands = await commandsMap.get(tab);
    return commands?.descriptorFront;
  },

  /**
   * This function allows us to ask if there is a known
   * descriptor for a tab without creating a descriptor.
   * @return true/false
   */
  isKnownTab(tab) {
    return commandsMap.has(tab);
  },
};
