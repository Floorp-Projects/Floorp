/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  TYPES: { LAST_PRIVATE_CONTEXT_EXIT },
} = require("resource://devtools/server/actors/resources/index.js");

class LastPrivateContextExitWatcher {
  #onAvailable;

  /**
   * Start watching for all times where we close a private browsing top level window.
   * Meaning we should clear the console for all logs generated from these private browsing contexts.
   *
   * @param WatcherActor watcherActor
   *        The watcher actor in the parent process from which we should
   *        observe these events.
   * @param Object options
   *        Dictionary object with following attributes:
   *        - onAvailable: mandatory function
   *          This will be called for each resource.
   */
  async watch(watcherActor, { onAvailable }) {
    this.#onAvailable = onAvailable;
    Services.obs.addObserver(this, "last-pb-context-exited");
  }

  observe(subject, topic) {
    if (topic === "last-pb-context-exited") {
      this.#onAvailable([
        {
          resourceType: LAST_PRIVATE_CONTEXT_EXIT,
        },
      ]);
    }
  }

  destroy() {
    Services.obs.removeObserver(this, "last-pb-context-exited");
  }
}

module.exports = LastPrivateContextExitWatcher;
