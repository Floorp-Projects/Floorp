/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createCommandsDictionary,
} = require("resource://devtools/shared/commands/index.js");
const { DevToolsLoader } = ChromeUtils.importESModule(
  "resource://devtools/shared/loader/Loader.sys.mjs"
);
loader.lazyRequireGetter(
  this,
  "DevToolsServer",
  "resource://devtools/server/devtools-server.js",
  true
);
// eslint-disable-next-line mozilla/reject-some-requires
loader.lazyRequireGetter(
  this,
  "DevToolsClient",
  "resource://devtools/client/devtools-client.js",
  true
);

/**
 * Functions for creating Commands for all debuggable contexts.
 *
 * All methods of this `CommandsFactory` object receive argument to describe to
 * which particular context we want to debug. And all returns a new instance of `commands` object.
 * Commands are implemented by modules defined in devtools/shared/commands.
 */
exports.CommandsFactory = {
  /**
   * Create commands for a given local tab.
   *
   * @param {Tab} tab: A local Firefox tab, running in this process.
   * @param {Object} options
   * @param {DevToolsClient} options.client: An optional DevToolsClient. If none is passed,
   *        a new one will be created.
   * @param {DevToolsClient} options.isWebExtension: An optional boolean to flag commands
   *        that are created for the WebExtension codebase.
   * @returns {Object} Commands
   */
  async forTab(tab, { client, isWebExtension } = {}) {
    if (!client) {
      client = await createLocalClient();
    }

    const descriptor = await client.mainRoot.getTab({ tab, isWebExtension });
    descriptor.doNotAttachThreadActor = isWebExtension;
    const commands = await createCommandsDictionary(descriptor);
    return commands;
  },

  /**
   * Chrome mochitest don't have access to any "tab",
   * so that the only way to attach to a fake tab is call RootFront.getTab
   * without any argument.
   */
  async forCurrentTabInChromeMochitest() {
    const client = await createLocalClient();
    const descriptor = await client.mainRoot.getTab();
    const commands = await createCommandsDictionary(descriptor);
    return commands;
  },

  /**
   * Create commands for the main process.
   *
   * @param {Object} options
   * @param {DevToolsClient} options.client: An optional DevToolsClient. If none is passed,
   *        a new one will be created.
   * @returns {Object} Commands
   */
  async forMainProcess({ client } = {}) {
    if (!client) {
      client = await createLocalClient();
    }

    const descriptor = await client.mainRoot.getMainProcess();
    const commands = await createCommandsDictionary(descriptor);
    return commands;
  },

  /**
   * Create commands for a given remote tab.
   *
   * Note that it can also be used for local tab, but isLocalTab attribute
   * on commands.descriptorFront will be false.
   *
   * @param {Number} browserId: Identify which tab we should create commands for.
   * @param {Object} options
   * @param {DevToolsClient} options.client: An optional DevToolsClient. If none is passed,
   *        a new one will be created.
   * @returns {Object} Commands
   */
  async forRemoteTab(browserId, { client } = {}) {
    if (!client) {
      client = await createLocalClient();
    }

    const descriptor = await client.mainRoot.getTab({ browserId });
    const commands = await createCommandsDictionary(descriptor);
    return commands;
  },

  /**
   * Create commands for a given main process worker.
   *
   * @param {String} id: WorkerDebugger's id, which is a unique ID computed by the platform code.
   *        These ids are exposed via WorkerDescriptor's id attributes.
   *        WorkerDescriptors can be retrieved via MainFront.listAllWorkers()/listWorkers().
   * @param {Object} options
   * @param {DevToolsClient} options.client: An optional DevToolsClient. If none is passed,
   *        a new one will be created.
   * @returns {Object} Commands
   */
  async forWorker(id, { client } = {}) {
    if (!client) {
      client = await createLocalClient();
    }

    const descriptor = await client.mainRoot.getWorker(id);
    const commands = await createCommandsDictionary(descriptor);
    return commands;
  },

  /**
   * Create commands for a Web Extension.
   *
   * @param {String} id The Web Extension ID to debug.
   * @param {Object} options
   * @param {DevToolsClient} options.client: An optional DevToolsClient. If none is passed,
   *        a new one will be created.
   * @returns {Object} Commands
   */
  async forAddon(id, { client } = {}) {
    if (!client) {
      client = await createLocalClient();
    }

    const descriptor = await client.mainRoot.getAddon({ id });
    const commands = await createCommandsDictionary(descriptor);
    return commands;
  },

  /**
   * This method will spawn a special `DevToolsClient`
   * which is meant to debug the same Firefox instance
   * and especially be able to debug chrome code.
   * The chrome code typically runs in the system principal.
   * This principal is a singleton which is shared among most Firefox internal codebase
   * (JSM, privileged html documents, JS-XPCOM,...)
   * In order to be able to debug these script we need to connect to a special DevToolsServer
   * that runs in a dedicated and distinct system principal which is different from
   * the one shared with the rest of Firefox frontend codebase.
   */
  async spawnClientToDebugSystemPrincipal() {
    // The Browser console ends up using the debugger in autocomplete.
    // Because the debugger can't be running in the same compartment than its debuggee,
    // we have to load the server in a dedicated Loader, flagged with
    // `freshCompartment`, which will force it to be loaded in another compartment.
    // We aren't using `invisibleToDebugger` in order to allow the Browser toolbox to
    // debug the Browser console. This is fine as they will spawn distinct Loaders and
    // so distinct `DevToolsServer` and actor modules.
    const customLoader = new DevToolsLoader({
      freshCompartment: true,
    });
    const { DevToolsServer: customDevToolsServer } = customLoader.require(
      "resource://devtools/server/devtools-server.js"
    );

    customDevToolsServer.init();

    // We want all the actors (root, browser and target-scoped) to be registered on the
    // DevToolsServer. This is needed so the Browser Console can retrieve:
    // - the console actors, which are target-scoped (See Bug 1416105)
    // - the screenshotActor, which is browser-scoped (for the `:screenshot` command)
    customDevToolsServer.registerAllActors();

    customDevToolsServer.allowChromeProcess = true;

    const client = new DevToolsClient(customDevToolsServer.connectPipe());
    await client.connect();

    return client;
  },

  /**
   * One method to handle the whole setup sequence to connect to RDP backend for the Browser Console.
   *
   * This will instantiate a special DevTools module loader for the DevToolsServer.
   * Then spawn a DevToolsClient to connect to it.
   * Get a Main Process Descriptor from it.
   * Finally spawn a commands object for this descriptor.
   */
  async forBrowserConsole() {
    // The Browser console ends up using the debugger in autocomplete.
    // Because the debugger can't be running in the same compartment than its debuggee,
    // we have to load the server in a dedicated Loader and so spawn a special client
    const client = await this.spawnClientToDebugSystemPrincipal();

    const descriptor = await client.mainRoot.getMainProcess();

    descriptor.doNotAttachThreadActor = true;

    // Force fetching the first top level target right away.
    await descriptor.getTarget();

    const commands = await createCommandsDictionary(descriptor);
    return commands;
  },
};

async function createLocalClient() {
  // Make sure the DevTools server is started.
  ensureDevToolsServerInitialized();

  // Create the client and connect it to the local server.
  const client = new DevToolsClient(DevToolsServer.connectPipe());
  await client.connect();

  return client;
}
// Also expose this method for tests which would like to create a client
// without involving commands. This would typically be tests against the Watcher actor
// and requires to prevent having TargetCommand from running.
// Or tests which are covering RootFront or global actor's fronts.
exports.createLocalClientForTests = createLocalClient;

function ensureDevToolsServerInitialized() {
  // Since a remote protocol connection will be made, let's start the
  // DevToolsServer here, once and for all tools.
  DevToolsServer.init();

  // Enable all the actors. We may not need all of them and registering
  // only root and target might be enough
  DevToolsServer.registerAllActors();

  // Enable being able to get child process actors
  // Same, this might not be useful
  DevToolsServer.allowChromeProcess = true;
}
