/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  TYPES: { SOURCE },
} = require("devtools/server/actors/resources/index");

/**
 * Start watching for all JS sources related to a given Target Actor.
 * This will notify about existing sources, but also the ones created in future.
 *
 * @param TargetActor targetActor
 *        The target actor from which we should observe sources
 * @param Object options
 *        Dictionary object with following attributes:
 *        - onAvailable: mandatory function
 *          This will be called for each resource.
 */
class SourceWatcher {
  constructor() {
    this.onNewSource = this.onNewSource.bind(this);
  }

  async watch(targetActor, { onAvailable }) {
    // Force attaching the target in order to ensure it instantiates the ThreadActor
    targetActor.attach();

    const { threadActor } = targetActor;
    this.threadActor = threadActor;
    this.onAvailable = onAvailable;

    // Disable `ThreadActor.newSource` RDP event in order to avoid unnecessary traffic
    threadActor.disableNewSourceEvents();

    threadActor.sourcesManager.on("newSource", this.onNewSource);

    // Before fetching all sources, process existing ones.
    // The ThreadActor is already up and running before this code runs
    // and have sources already registered and for which newSource event already fired.
    onAvailable(
      threadActor.sourcesManager.iter().map(s => {
        const resource = s.form();
        resource.resourceType = SOURCE;
        return resource;
      })
    );

    // Requesting all sources should end up emitting newSource on threadActor.sourcesManager
    threadActor.addAllSources();
  }

  /**
   * Stop watching for sources
   */
  destroy() {
    this.threadActor.sourcesManager.off("newSource", this.onNewSource);
  }

  onNewSource(source) {
    const resource = source.form();
    resource.resourceType = SOURCE;
    this.onAvailable([resource]);
  }
}

module.exports = SourceWatcher;
