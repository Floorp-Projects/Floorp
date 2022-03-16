/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyImporter(
  this,
  "ExtensionParent",
  "resource://gre/modules/ExtensionParent.jsm"
);

const {
  TYPES: { EXTENSIONS_BGSCRIPT_STATUS },
} = require("devtools/server/actors/resources/index");

class ExtensionsBackgroundScriptStatusWatcher {
  /**
   * Start watching for the status updates related to a background
   * scripts extension context (either an event page or a background
   * service worker).
   *
   * This is used in about:debugging to update the background script
   * row updated visible in Extensions details cards (only for extensions
   * with a non persistent background script defined in the manifest)
   * when the background contex is terminated on idle or started back
   * to handle a persistent WebExtensions API event.
   *
   * @param RootActor rootActor
   *        The root actor in the parent process from which we should
   *        observe root resources.
   * @param Object options
   *        Dictionary object with following attributes:
   *        - onAvailable: mandatory function
   *          This will be called for each resource.
   *        - onUpdated: optional function
   *          This would be called multiple times for each resource.
   */
  async watch(rootActor, { onAvailable, onUpdated }) {
    this.rootActor = rootActor;
    this.onAvailable = onAvailable;
    this.onUpdated = onUpdated;

    this.unwatchBackgroundStatusUpdates = ExtensionParent.DebugUtils.watchBackgroundScriptStatusUpdates(
      this.onBackgroundScriptStatus
    );
  }

  onBackgroundScriptStatus = (addonId, isRunning) => {
    this.onAvailable([
      {
        resourceType: EXTENSIONS_BGSCRIPT_STATUS,
        payload: {
          addonId,
          isRunning,
        },
      },
    ]);
  };

  destroy() {
    this.unwatchBackgroundStatusUpdates?.();
  }
}

module.exports = ExtensionsBackgroundScriptStatusWatcher;
