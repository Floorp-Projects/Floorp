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

const targets = new WeakMap();

/**
 * Functions for creating Targets
 */
exports.TargetFactory = {
  /**
   * Construct a Target. The target will be cached for each Tab so that we create only
   * one per tab.
   *
   * @param {XULTab} tab
   *        The tab to use in creating a new target.
   * @param {DevToolsClient} client
   *        Optional client to fetch the target actor from.
   *
   * @return A target object
   */
  forTab: async function(tab, client) {
    let target = targets.get(tab);
    if (target) {
      return target;
    }
    const promise = this.createTargetForTab(tab, client);
    // Immediately set the target's promise in cache to prevent race
    targets.set(tab, promise);
    target = await promise;
    // Then replace the promise with the target object
    targets.set(tab, target);
    target.once("close", () => {
      targets.delete(tab);
    });
    return target;
  },

  /**
   * Instantiate a target for the given tab.
   *
   * This will automatically:
   * - if no client is passed, spawn a DevToolsServer in the parent process,
   *   and create a DevToolsClient and connect it to this local DevToolsServer,
   * - call RootActor's `getTab` request to retrieve the FrameTargetActor's form,
   * - instantiate a Target instance.
   *
   * @param {XULTab} tab
   *        The tab to use in creating a new target.
   * @param {DevToolsClient} client
   *        Optional client to fetch the target actor from.
   *
   * @return A target object
   */
  async createTargetForTab(tab, client) {
    function createLocalServer() {
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
    }

    function createLocalClient() {
      createLocalServer();
      return new DevToolsClient(DevToolsServer.connectPipe());
    }

    if (!client) {
      client = createLocalClient();

      // Connect the local client to the local server
      await client.connect();
    }

    // Fetch the FrameTargetActor's Front which is a BrowsingContextTargetFront
    const descriptor = await client.mainRoot.getTab({ tab });
    return descriptor.getTarget();
  },

  /**
   * Creating a target for a tab that is being closed is a problem because it
   * allows a leak as a result of coming after the close event which normally
   * clears things up. This function allows us to ask if there is a known
   * target for a tab without creating a target
   * @return true/false
   */
  isKnownTab: function(tab) {
    return targets.has(tab);
  },
};
