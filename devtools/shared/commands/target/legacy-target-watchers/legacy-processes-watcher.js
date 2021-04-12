/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class LegacyProcessesWatcher {
  constructor(targetCommand, onTargetAvailable, onTargetDestroyed) {
    this.targetCommand = targetCommand;
    this.rootFront = targetCommand.rootFront;

    this.onTargetAvailable = onTargetAvailable;
    this.onTargetDestroyed = onTargetDestroyed;

    this.descriptors = new Set();
    this._processListChanged = this._processListChanged.bind(this);
  }

  async _processListChanged() {
    if (this.targetCommand.isDestroyed()) {
      return;
    }

    const processes = await this.rootFront.listProcesses();
    // Process the new list to detect the ones being destroyed
    // Force destroyed the descriptor as well as the target
    for (const descriptor of this.descriptors) {
      if (!processes.includes(descriptor)) {
        // Manually call onTargetDestroyed listeners in order to
        // ensure calling them *before* destroying the descriptor.
        // Otherwise the descriptor will automatically destroy the target
        // and may not fire the contentProcessTarget's destroy event.
        const target = descriptor.getCachedTarget();
        if (target) {
          this.onTargetDestroyed(target);
        }

        descriptor.destroy();
        this.descriptors.delete(descriptor);
      }
    }

    const promises = processes
      .filter(descriptor => !this.descriptors.has(descriptor))
      .map(async descriptor => {
        // Add the new process descriptors to the local list
        this.descriptors.add(descriptor);
        const target = await descriptor.getTarget();
        if (!target) {
          console.error(
            "Wasn't able to retrieve the target for",
            descriptor.actorID
          );
          return;
        }
        await this.onTargetAvailable(target);
      });

    await Promise.all(promises);
  }

  async listen() {
    this.rootFront.on("processListChanged", this._processListChanged);
    await this._processListChanged();
  }

  unlisten() {
    this.rootFront.off("processListChanged", this._processListChanged);
  }
}

module.exports = { LegacyProcessesWatcher };
