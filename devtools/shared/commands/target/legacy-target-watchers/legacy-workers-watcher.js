/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  LegacyProcessesWatcher,
} = require("devtools/shared/commands/target/legacy-target-watchers/legacy-processes-watcher");

class LegacyWorkersWatcher {
  constructor(targetCommand, onTargetAvailable, onTargetDestroyed) {
    this.targetCommand = targetCommand;
    this.rootFront = targetCommand.rootFront;

    this.onTargetAvailable = onTargetAvailable;
    this.onTargetDestroyed = onTargetDestroyed;

    this.targetsByProcess = new WeakMap();
    this.targetsListeners = new WeakMap();

    this._onProcessAvailable = this._onProcessAvailable.bind(this);
    this._onProcessDestroyed = this._onProcessDestroyed.bind(this);
  }

  async _onProcessAvailable({ targetFront }) {
    this.targetsByProcess.set(targetFront, new Set());
    // Listen for worker which will be created later
    const listener = this._workerListChanged.bind(this, targetFront);
    this.targetsListeners.set(targetFront, listener);

    // If this is the browser toolbox, we have to listen from the RootFront
    // (see comment in _workerListChanged)
    const front = targetFront.isParentProcess ? this.rootFront : targetFront;
    front.on("workerListChanged", listener);

    // We also need to process the already existing workers
    await this._workerListChanged(targetFront);
  }

  async _onProcessDestroyed({ targetFront }) {
    const existingTargets = this.targetsByProcess.get(targetFront);

    // Process the new list to detect the ones being destroyed
    // Force destroying the targets
    for (const target of existingTargets) {
      this.onTargetDestroyed(target);

      target.destroy();
      existingTargets.delete(target);
    }
    this.targetsByProcess.delete(targetFront);
    this.targetsListeners.delete(targetFront);
  }

  _supportWorkerTarget(workerTarget) {
    // subprocess workers are ignored because they take several seconds to
    // attach to when opening the browser toolbox. See bug 1594597.
    // When attaching we get the following error:
    // JavaScript error: resource://devtools/server/startup/worker.js,
    //   line 37: NetworkError: WorkerDebuggerGlobalScope.loadSubScript: Failed to load worker script at resource://devtools/shared/worker/loader.js (nsresult = 0x805e0006)
    return (
      workerTarget.isDedicatedWorker &&
      !workerTarget.url.startsWith(
        "resource://gre/modules/subprocess/subprocess_worker"
      )
    );
  }

  async _workerListChanged(targetFront) {
    // If we're in the Browser Toolbox, query workers from the Root Front instead of the
    // ParentProcessTarget as the ParentProcess Target filters out the workers to only
    // show the one from the top level window, whereas we expect the one from all the
    // windows, and also the window-less ones.
    // TODO: For Content Toolbox, expose SW of the page, maybe optionally?
    const front = targetFront.isParentProcess ? this.rootFront : targetFront;
    if (!front || front.isDestroyed() || this.targetCommand.isDestroyed()) {
      return;
    }

    let workers;
    try {
      ({ workers } = await front.listWorkers());
    } catch (e) {
      // Workers may be added/removed at anytime so that listWorkers request
      // can be spawn during a toolbox destroy sequence and easily fail
      if (front.isDestroyed()) {
        return;
      }
      throw e;
    }

    // Fetch the list of already existing worker targets for this process target front.
    const existingTargets = this.targetsByProcess.get(targetFront);
    if (!existingTargets) {
      // unlisten was called while processing the workerListChanged callback.
      return;
    }

    // Process the new list to detect the ones being destroyed
    // Force destroying the targets
    for (const target of existingTargets) {
      if (!workers.includes(target)) {
        this.onTargetDestroyed(target);

        target.destroy();
        existingTargets.delete(target);
      }
    }

    const promises = workers.map(workerTarget =>
      this._processNewWorkerTarget(workerTarget, existingTargets)
    );
    await Promise.all(promises);
  }

  // This is overloaded for Service Workers, which records all SW targets,
  // but only notify about a subset of them.
  _recordWorkerTarget(workerTarget) {
    return this._supportWorkerTarget(workerTarget);
  }

  async _processNewWorkerTarget(workerTarget, existingTargets) {
    if (
      !this._recordWorkerTarget(workerTarget) ||
      existingTargets.has(workerTarget) ||
      this.targetCommand.isDestroyed()
    ) {
      return;
    }

    // Add the new worker targets to the local list
    existingTargets.add(workerTarget);

    if (this._supportWorkerTarget(workerTarget)) {
      await this.onTargetAvailable(workerTarget);
    }
  }

  async listen() {
    // Listen to the current target front.
    this.target = this.targetCommand.targetFront;

    if (this.target.isParentProcess) {
      await this.targetCommand.watchTargets(
        [this.targetCommand.TYPES.PROCESS],
        this._onProcessAvailable,
        this._onProcessDestroyed
      );

      // The ParentProcessTarget front is considered to be a FRAME instead of a PROCESS.
      // So process it manually here.
      await this._onProcessAvailable({ targetFront: this.target });
      return;
    }

    if (this._isSharedWorkerWatcher) {
      // Here we're not in the browser toolbox, and SharedWorker targets are not supported
      // in regular toolbox (See Bug 1607778)
      return;
    }

    if (this._isServiceWorkerWatcher) {
      this._legacyProcessesWatcher = new LegacyProcessesWatcher(
        this.targetCommand,
        async targetFront => {
          // Service workers only live in content processes.
          if (!targetFront.isParentProcess) {
            await this._onProcessAvailable({ targetFront });
          }
        },
        targetFront => {
          if (!targetFront.isParentProcess) {
            this._onProcessDestroyed({ targetFront });
          }
        }
      );
      await this._legacyProcessesWatcher.listen();
      return;
    }

    // Here, we're handling Dedicated Workers in content toolbox.
    this.targetsByProcess.set(this.target, new Set());
    this._workerListChangedListener = this._workerListChanged.bind(
      this,
      this.target
    );
    this.target.on("workerListChanged", this._workerListChangedListener);
    await this._workerListChanged(this.target);
  }

  _getProcessTargets() {
    return this.targetCommand.getAllTargets([this.targetCommand.TYPES.PROCESS]);
  }

  unlisten() {
    // Stop listening for new process targets.
    if (this.target.isParentProcess) {
      this.targetCommand.unwatchTargets(
        [this.targetCommand.TYPES.PROCESS],
        this._onProcessAvailable,
        this._onProcessDestroyed
      );
    } else if (this._isServiceWorkerWatcher) {
      this._legacyProcessesWatcher.unlisten();
    }

    // Cleanup the targetsByProcess/targetsListeners maps, and unsubscribe from
    // all targetFronts. Process target fronts are either stored locally when
    // watching service workers for the content toolbox, or can be retrieved via
    // the TargetCommand API otherwise (see _getProcessTargets implementations).
    if (this.target.isParentProcess || this._isServiceWorkerWatcher) {
      for (const targetFront of this._getProcessTargets()) {
        const listener = this.targetsListeners.get(targetFront);
        targetFront.off("workerListChanged", listener);
        this.targetsByProcess.delete(targetFront);
        this.targetsListeners.delete(targetFront);
      }
    } else {
      this.target.off("workerListChanged", this._workerListChangedListener);
      delete this._workerListChangedListener;
      this.targetsByProcess.delete(this.target);
      this.targetsListeners.delete(this.target);
    }
  }
}

module.exports = { LegacyWorkersWatcher };
