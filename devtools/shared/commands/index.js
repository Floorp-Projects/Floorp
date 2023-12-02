/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// List of all command modules
// (please try to keep the list alphabetically sorted)
/* eslint sort-keys: "error" */
/* eslint-enable sort-keys */
const Commands = {
  inspectedWindowCommand:
    "devtools/shared/commands/inspected-window/inspected-window-command",
  inspectorCommand: "devtools/shared/commands/inspector/inspector-command",
  networkCommand: "devtools/shared/commands/network/network-command",
  resourceCommand: "devtools/shared/commands/resource/resource-command",
  rootResourceCommand:
    "devtools/shared/commands/root-resource/root-resource-command",
  scriptCommand: "devtools/shared/commands/script/script-command",
  targetCommand: "devtools/shared/commands/target/target-command",
  targetConfigurationCommand:
    "devtools/shared/commands/target-configuration/target-configuration-command",
  threadConfigurationCommand:
    "devtools/shared/commands/thread-configuration/thread-configuration-command",
  tracerCommand: "devtools/shared/commands/tracer/tracer-command",
};
/* eslint-disable sort-keys */

/**
 * For a given descriptor and its related Targets, already initialized,
 * return the dictionary with all command instances.
 * This dictionary is lazy and commands will be loaded and instanciated on-demand.
 */
async function createCommandsDictionary(descriptorFront) {
  // Bug 1675763: Watcher actor is not available in all situations yet.
  let watcherFront;
  const supportsWatcher = descriptorFront.traits?.watcher;
  if (supportsWatcher) {
    watcherFront = await descriptorFront.getWatcher();
  }
  const { client } = descriptorFront;

  const allInstantiatedCommands = new Set();

  const dictionary = {
    // Expose both client and descriptor for legacy codebases, or tests.
    // But ideally only commands should interact with these two objects
    client,
    descriptorFront,
    watcherFront,

    // Expose for tests
    waitForRequestsToSettle() {
      return descriptorFront.client.waitForRequestsToSettle();
    },

    // Boolean flag to know if the DevtoolsClient should be closed
    // when this commands happens to be destroyed.
    // This is set by:
    // * commands-from-url in case we are opening a toolbox
    //   with a dedicated DevToolsClient (mostly from about:debugging, when the client isn't "cached").
    // * CommandsFactory, when we are connecting to a local tab and expect
    //   the client, toolbox and descriptor to all follow the same lifecycle.
    shouldCloseClient: true,

    /**
     * Destroy the commands which will destroy:
     * - all inner commands,
     * - the related descriptor,
     * - the related DevToolsClient (not always)
     */
    async destroy() {
      descriptorFront.off("descriptor-destroyed", this.destroy);

      // Destroy all inner command modules
      for (const command of allInstantiatedCommands) {
        if (typeof command.destroy == "function") {
          command.destroy();
        }
      }
      allInstantiatedCommands.clear();

      // Destroy the descriptor front, and all its children fronts.
      // Watcher, targets,...
      //
      // Note that DescriptorFront.destroy will be null because of Pool.destroy
      // when this function is called while the descriptor front itself is being
      // destroyed.
      if (!descriptorFront.isDestroyed()) {
        await descriptorFront.destroy();
      }

      // Close the DevToolsClient. Shutting down the connection
      // to the debuggable context and its DevToolsServer.
      //
      // See shouldCloseClient jsdoc about this condition.
      if (this.shouldCloseClient) {
        await client.close();
      }
    },
  };
  dictionary.destroy = dictionary.destroy.bind(dictionary);

  // Automatically destroy the commands object if the descriptor
  // happens to be destroyed. Which means that the debuggable context
  // is no longer debuggable.
  descriptorFront.on("descriptor-destroyed", dictionary.destroy);

  for (const name in Commands) {
    loader.lazyGetter(dictionary, name, () => {
      const Constructor = require(Commands[name]);
      const command = new Constructor({
        // Commands can use other commands
        commands: dictionary,

        // The context to inspect identified by this descriptor
        descriptorFront,

        // The front for the Watcher Actor, related to the given descriptor
        // This is a key actor to watch for targets and resources and pull global actors running in the parent process
        watcherFront,

        // From here, we could pass DevToolsClient, or any useful protocol classes...
        // so that we abstract where and how to fetch all necessary interfaces
        // and avoid having to know that you might pull the client via descriptorFront.client
      });
      allInstantiatedCommands.add(command);
      return command;
    });
  }

  return dictionary;
}
exports.createCommandsDictionary = createCommandsDictionary;
