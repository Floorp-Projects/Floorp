/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyRequireGetter(
  this,
  "DebuggerServer",
  "devtools/server/debugger-server",
  true
);
loader.lazyRequireGetter(
  this,
  "DebuggerClient",
  "devtools/shared/client/debugger-client",
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
   *
   * @return A target object
   */
  forTab: async function(tab) {
    let target = targets.get(tab);
    if (target) {
      return target;
    }
    const promise = this.createTargetForTab(tab);
    // Immediately set the target's promise in cache to prevent race
    targets.set(tab, promise);
    target = await promise;
    // Then replace the promise with the target object
    targets.set(tab, target);
    target.attachTab(tab);
    target.once("close", () => {
      targets.delete(tab);
    });
    return target;
  },

  /**
   * Instantiate a target for the given tab.
   *
   * This will automatically:
   * - spawn a DebuggerServer in the parent process,
   * - create a DebuggerClient and connect it to this local DebuggerServer,
   * - call RootActor's `getTab` request to retrieve the FrameTargetActor's form,
   * - instantiate a Target instance.
   *
   * @param {XULTab} tab
   *        The tab to use in creating a new target.
   *
   * @return A target object
   */
  async createTargetForTab(tab) {
    function createLocalServer() {
      // Since a remote protocol connection will be made, let's start the
      // DebuggerServer here, once and for all tools.
      DebuggerServer.init();

      // When connecting to a local tab, we only need the root actor.
      // Then we are going to call frame-connector.js' connectToFrame and talk
      // directly with actors living in the child process.
      // We also need browser actors for actor registry which enabled addons
      // to register custom actors.
      // TODO: the comment and implementation are out of sync here. See Bug 1420134.
      DebuggerServer.registerAllActors();
      // Enable being able to get child process actors
      DebuggerServer.allowChromeProcess = true;
    }

    function createLocalClient() {
      return new DebuggerClient(DebuggerServer.connectPipe());
    }

    createLocalServer();
    const client = createLocalClient();

    // Connect the local client to the local server
    await client.connect();

    // Fetch the FrameTargetActor's Front
    return client.mainRoot.getTab({ tab });
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
