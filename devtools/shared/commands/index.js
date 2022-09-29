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
  resourceCommand: "devtools/shared/commands/resource/resource-command",
  rootResourceCommand:
    "devtools/shared/commands/root-resource/root-resource-command",
  scriptCommand: "devtools/shared/commands/script/script-command",
  targetCommand: "devtools/shared/commands/target/target-command",
  targetConfigurationCommand:
    "devtools/shared/commands/target-configuration/target-configuration-command",
  threadConfigurationCommand:
    "devtools/shared/commands/thread-configuration/thread-configuration-command",
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

    // We want to keep destroy being defined last
    async destroy() {
      for (const command of allInstantiatedCommands) {
        if (typeof command.destroy == "function") {
          command.destroy();
        }
      }
      allInstantiatedCommands.clear();
      await descriptorFront.destroy();
      await client.close();
    },
  };

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
