/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyRequireGetter(
  this,
  "DevToolsServer",
  "devtools/server/devtools-server",
  true
);
loader.lazyRequireGetter(
  this,
  "DevToolsClient",
  "devtools/client/devtools-client",
  true
);

// Map of existing TabDescriptors, keyed by XULTab.
const descriptors = new WeakMap();

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
   * - call RootActor's `getTab` request to retrieve the FrameTargetActor's form
   *
   * @param {XULTab} tab
   *        The tab to use in creating a new descriptor.
   *
   * @return {TabDescriptorFront} The tab descriptor for the provided tab.
   */
  async createDescriptorForTab(tab) {
    let descriptor = descriptors.get(tab);
    if (descriptor) {
      return descriptor;
    }

    const promise = this._createDescriptorForTab(tab);
    // Immediately set the descriptor's promise in cache to prevent race
    descriptors.set(tab, promise);
    descriptor = await promise;
    // Then replace the promise with the descriptor object
    descriptors.set(tab, descriptor);

    descriptor.once("descriptor-destroyed", () => {
      descriptors.delete(tab);
    });
    return descriptor;
  },

  async _createDescriptorForTab(tab) {
    // Make sure the DevTools server is started.
    this._ensureDevToolsServerInitialized();

    // Create the client and connect it to the local server.
    const client = new DevToolsClient(DevToolsServer.connectPipe());
    await client.connect();

    return client.mainRoot.getTab({ tab });
  },

  _ensureDevToolsServerInitialized() {
    // Since a remote protocol connection will be made, let's start the
    // DevToolsServer here, once and for all tools.
    DevToolsServer.init();

    // When connecting to a local tab, we only need the root actor.
    // Then we are going to call frame-connector.js' connectToFrame and talk
    // directly with actors living in the child process.
    // We also need browser actors for actor registry which enabled addons
    // to register custom actors.
    // TODO: the comment and implementation are out of sync here. See Bug 1420134.
    DevToolsServer.registerAllActors();
    // Enable being able to get child process actors
    DevToolsServer.allowChromeProcess = true;
  },

  /**
   * Retrieve an existing descriptor created by this factory for the provided
   * tab. Returns null if no descriptor was created yet.
   *
   * @param {XULTab} tab
   *        The tab for which the descriptor should be retrieved
   */
  async getDescriptorForTab(tab) {
    // descriptors.get(tab) can either return an initialized descriptor, a promise
    // which will resolve a descriptor, or null if no descriptor was ever created
    // for this tab.
    const descriptor = await descriptors.get(tab);
    return descriptor;
  },

  /**
   * This function allows us to ask if there is a known
   * descriptor for a tab without creating a descriptor.
   * @return true/false
   */
  isKnownTab: function(tab) {
    return descriptors.has(tab);
  },
};
