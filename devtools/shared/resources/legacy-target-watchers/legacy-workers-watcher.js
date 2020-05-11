/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyRequireGetter(
  this,
  "TargetList",
  "devtools/shared/resources/target-list",
  true
);

class LegacyWorkersWatcher {
  constructor(targetList, onTargetAvailable, onTargetDestroyed) {
    this.targetList = targetList;
    this.rootFront = targetList.rootFront;
    this.target = targetList.targetFront;

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
    const { workers } = await front.listWorkers();

    // Fetch the list of already existing worker targets for this process target front.
    const existingTargets = this.targetsByProcess.get(targetFront);

    // Process the new list to detect the ones being destroyed
    // Force destroying the targets
    for (const target of existingTargets) {
      if (!workers.includes(target)) {
        this.onTargetDestroyed(target);

        target.destroy();
        existingTargets.delete(target);
      }
    }

    const promises = [];
    for (const workerTarget of workers) {
      if (
        !this._supportWorkerTarget(workerTarget) ||
        existingTargets.has(workerTarget)
      ) {
        continue;
      }

      // Add the new worker targets to the local list
      existingTargets.add(workerTarget);
      promises.push(this.onTargetAvailable(workerTarget));
    }

    await Promise.all(promises);
  }

  async listen() {
    if (this.target.isParentProcess) {
      await this.targetList.watchTargets(
        [TargetList.TYPES.PROCESS],
        this._onProcessAvailable,
        this._onProcessDestroyed
      );
      // The ParentProcessTarget front is considered to be a FRAME instead of a PROCESS.
      // So process it manually here.
      await this._onProcessAvailable({ targetFront: this.target });
    } else {
      this.targetsByProcess.set(this.target, new Set());
      this._workerListChangedListener = this._workerListChanged.bind(
        this,
        this.target
      );
      this.target.on("workerListChanged", this._workerListChangedListener);
      await this._workerListChanged(this.target);
    }
  }

  unlisten() {
    if (this.target.isParentProcess) {
      for (const targetFront of this.targetList.getAllTargets(
        TargetList.TYPES.PROCESS
      )) {
        const listener = this.targetsListeners.get(targetFront);
        targetFront.off("workerListChanged", listener);
        this.targetsByProcess.delete(targetFront);
        this.targetsListeners.delete(targetFront);
      }
      this.targetList.unwatchTargets(
        [TargetList.TYPES.PROCESS],
        this._onProcessAvailable,
        this._onProcessDestroyed
      );
    } else {
      this.target.off("workerListChanged", this._workerListChangedListener);
      delete this._workerListChangedListener;
      this.targetsByProcess.delete(this.target);
      this.targetsListeners.delete(this.target);
    }
  }
}

module.exports = { LegacyWorkersWatcher };
