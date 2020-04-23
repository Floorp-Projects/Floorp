/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const EventEmitter = require("devtools/shared/event-emitter");

const BROWSERTOOLBOX_FISSION_ENABLED = "devtools.browsertoolbox.fission";
const CONTENTTOOLBOX_FISSION_ENABLED = "devtools.contenttoolbox.fission";

// Intermediate components which implement the watch + unwatch
// using existing listFoo methods and fooListChanged events.
// The plan here is to followup to implement listen and unlisten
// methods directly on the target fronts. This code would then
// become the backward compatibility code which we could later remove.
class LegacyImplementationProcesses {
  constructor(targetList, onTargetAvailable, onTargetDestroyed) {
    this.targetList = targetList;
    this.rootFront = targetList.rootFront;
    this.target = targetList.targetFront;

    this.onTargetAvailable = onTargetAvailable;
    this.onTargetDestroyed = onTargetDestroyed;

    this.descriptors = new Set();
    this._processListChanged = this._processListChanged.bind(this);
  }

  async _processListChanged() {
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

class LegacyImplementationFrames {
  constructor(targetList, onTargetAvailable) {
    this.targetList = targetList;
    this.rootFront = targetList.rootFront;
    this.target = targetList.targetFront;

    this.onTargetAvailable = onTargetAvailable;
  }

  async listen() {
    // Note that even if we are calling listRemoteFrames on `this.target`, this ends up
    // being forwarded to the RootFront. So that the Descriptors are managed
    // by RootFront.
    // TODO: support frame listening. For now, this only fetches already existing targets
    const { frames } = await this.target.listRemoteFrames();

    const promises = frames
      .filter(
        // As we listen for frameDescriptor's on the RootFront, we get
        // all the frames and not only the one related to the given `target`.
        // TODO: support deeply nested frames
        descriptor =>
          descriptor.parentID == this.target.browsingContextID ||
          descriptor.id == this.target.browsingContextID
      )
      .map(async descriptor => {
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

  unlisten() {}
}

class LegacyImplementationWorkers {
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
    if (targetFront.isParentProcess) {
      this.rootFront.on("workerListChanged", listener);
    } else {
      targetFront.on("workerListChanged", listener);
    }
    // But also process the already existing ones
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

  async _workerListChanged(targetFront) {
    let workers;
    // Browser Toolbox codepath
    if (targetFront.isParentProcess) {
      // Query workers from the Root Front instead of the ParentProcessTarget
      // as the ParentProcess Target filters out the workers to only show the one from the top level window,
      // whereas we expect the one from all the windows, and also the window-less ones.
      ({ workers } = await this.rootFront.listWorkers());
    } else {
      // Content Toolbox codepath
      //TODO: expose SW of the page, maybe optionally?
      ({ workers } = await targetFront.listWorkers());
    }

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
    const promises = workers
      // subprocess workers are ignored because they take several seconds to
      // attach to when opening the browser toolbox. See bug 1594597.
      // When attaching we get the following error:
      // JavaScript error: resource://devtools/server/startup/worker.js, line 37: NetworkError: WorkerDebuggerGlobalScope.loadSubScript: Failed to load worker script at resource://devtools/shared/worker/loader.js (nsresult = 0x805e0006)
      .filter(
        workerTarget =>
          !workerTarget.url.startsWith(
            "resource://gre/modules/subprocess/subprocess_worker"
          )
      )
      .filter(workerTarget => !existingTargets.has(workerTarget))
      .map(async workerTarget => {
        // Add the new worker targets to the local list
        existingTargets.add(workerTarget);
        await this.onTargetAvailable(workerTarget);
      });

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

class TargetList {
  /**
   * This class helps managing, iterating over and listening for Targets.
   *
   * It exposes:
   *  - the top level target, typically the main process target for the browser toolbox
   *    or the browsing context target for a regular web toolbox
   *  - target of remoted iframe, in case Fission is enabled and some <iframe>
   *    are running in a distinct process
   *  - target switching. If the top level target changes for a new one,
   *    all the targets are going to be declared as destroyed and the new ones
   *    will be notified to the user of this API.
   *
   * @param {RootFront} rootFront
   *        The root front.
   * @param {TargetFront} targetFront
   *        The top level target to debug. Note that in case of target switching,
   *        this may be replaced by a new one over time.
   */
  constructor(rootFront, targetFront) {
    this.rootFront = rootFront;
    // Note that this is a public attribute, used outside of this class
    // and helps knowing what is the current top level target we debug.
    this.targetFront = targetFront;

    // Reports if we have at least one listener for the given target type
    this._listenersStarted = new Set();

    // List of all the target fronts
    this._targets = new Set();
    this._targets.add(targetFront);

    // Listeners for target creation and destruction
    this._createListeners = new EventEmitter();
    this._destroyListeners = new EventEmitter();

    this._onTargetAvailable = this._onTargetAvailable.bind(this);
    this._onTargetDestroyed = this._onTargetDestroyed.bind(this);

    this.legacyImplementation = {
      process: new LegacyImplementationProcesses(
        this,
        this._onTargetAvailable,
        this._onTargetDestroyed
      ),
      frame: new LegacyImplementationFrames(
        this,
        this._onTargetAvailable,
        this._onTargetDestroyed
      ),
      worker: new LegacyImplementationWorkers(
        this,
        this._onTargetAvailable,
        this._onTargetDestroyed
      ),
    };

    // Public flag to allow listening for workers even if the fission pref is off
    // This allows listening for workers in the content toolbox outside of fission contexts
    // For now, this is only toggled by tests.
    this.listenForWorkers = false;
  }

  // Called whenever a new Target front is available.
  // Either because a target was already available as we started calling startListening
  // or if it has just been created
  async _onTargetAvailable(targetFront, isTargetSwitching = false) {
    if (this._targets.has(targetFront)) {
      // The top level target front can be reported via listRemoteFrames as well as listProcesses
      // in the case of the BrowserToolbox. For any other target, log an error if it is already
      // registered.
      if (targetFront != this.targetFront) {
        console.error(
          "Target is already registered in the TargetList",
          targetFront.actorID
        );
      }
      return;
    }

    this._targets.add(targetFront);

    // Map the descriptor typeName to a target type.
    const targetType = this.getTargetType(targetFront);

    // Notify the target front creation listeners
    await this._createListeners.emitAsync(targetType, {
      type: targetType,
      targetFront,
      isTopLevel: targetFront == this.targetFront,
      isTargetSwitching,
    });
  }

  _onTargetDestroyed(targetFront, isTargetSwitching = false) {
    const targetType = this.getTargetType(targetFront);
    this._destroyListeners.emit(targetType, {
      type: targetType,
      targetFront,
      isTopLevel: targetFront == this.targetFront,
      isTargetSwitching,
    });
    this._targets.delete(targetFront);
  }

  _setListening(type, value) {
    if (value) {
      this._listenersStarted.add(type);
    } else {
      this._listenersStarted.delete(type);
    }
  }

  _isListening(type) {
    return this._listenersStarted.has(type);
  }

  /**
   * Interact with the actors in order to start listening for new types of targets.
   * This will fire the _onTargetAvailable function for all already-existing targets,
   * as well as the next one to be created. It will also call _onTargetDestroyed
   * everytime a target is reported as destroyed by the actors.
   * By the time this function resolves, all the already-existing targets will be
   * reported to _onTargetAvailable.
   */
  async startListening() {
    let types = [];
    if (this.targetFront.isParentProcess) {
      const fissionBrowserToolboxEnabled = Services.prefs.getBoolPref(
        BROWSERTOOLBOX_FISSION_ENABLED
      );
      if (fissionBrowserToolboxEnabled) {
        types = TargetList.ALL_TYPES;
      }
    } else if (this.targetFront.isLocalTab) {
      const fissionContentToolboxEnabled = Services.prefs.getBoolPref(
        CONTENTTOOLBOX_FISSION_ENABLED
      );
      if (fissionContentToolboxEnabled) {
        types = [TargetList.TYPES.FRAME];
      }
    }
    if (this.listenForWorkers && !types.includes(TargetList.TYPES.WORKER)) {
      types.push(TargetList.TYPES.WORKER);
    }
    // If no pref are set to true, nor is listenForWorkers set to true,
    // we won't listen for any additional target. Only the top level target
    // will be managed. We may still do target-switching.

    for (const type of types) {
      if (this._isListening(type)) {
        continue;
      }
      this._setListening(type, true);

      if (this.legacyImplementation[type]) {
        await this.legacyImplementation[type].listen();
      } else {
        // TO BE IMPLEMENTED via this.targetFront.watchFronts(type)
        // For now we always go throught "legacy" codepath.
        throw new Error(`Unsupported target type '${type}'`);
      }
    }
  }

  stopListening() {
    for (const type of TargetList.ALL_TYPES) {
      if (!this._isListening(type)) {
        continue;
      }
      this._setListening(type, false);

      if (this.legacyImplementation[type]) {
        this.legacyImplementation[type].unlisten();
      } else {
        // TO BE IMPLEMENTED via this.targetFront.unwatchFronts(type)
        // For now we always go throught "legacy" codepath.
        throw new Error(`Unsupported target type '${type}'`);
      }
    }
  }

  getTargetType(target) {
    const { typeName } = target;
    if (typeName == "browsingContextTarget") {
      return TargetList.TYPES.FRAME;
    } else if (
      typeName == "contentProcessTarget" ||
      typeName == "parentProcessTarget"
    ) {
      return TargetList.TYPES.PROCESS;
    } else if (typeName == "workerTarget") {
      return TargetList.TYPES.WORKER;
    }
    throw new Error("Unsupported target typeName: " + typeName);
  }

  _matchTargetType(type, target) {
    return type === this.getTargetType(target);
  }

  /**
   * Listen for the creation and/or destruction of target fronts matching one of the provided types.
   *
   * @param {Array<String>} types
   *        The type of target to listen for. Constant of TargetList.TYPES.
   * @param {Function} onAvailable
   *        Callback fired when a target has been just created or was already available.
   *        The function is called with three arguments:
   *        - {String} type: The target type
   *        - {TargetFront} targetFront: The target Front
   *        - {Boolean} isTopLevel: Is this target the top level one?
   *        - {Boolean} isTargetSwitching: Is this target relates to a navigation and
   *                    this replaced a previously available target, this flag will be true
   * @param {Function} onDestroy
   *        Callback fired in case of target front destruction.
   *        The function is called with the same arguments than onAvailable.
   */
  async watchTargets(types, onAvailable, onDestroy) {
    if (typeof onAvailable != "function") {
      throw new Error(
        "TargetList.watchTargets expects a function as second argument"
      );
    }

    // Notify about already existing target of these types
    const promises = [...this._targets]
      .filter(targetFront => {
        const targetType = this.getTargetType(targetFront);
        return types.includes(targetType);
      })
      .map(async targetFront => {
        try {
          // Ensure waiting for eventual async create listeners
          // which may setup things regarding the existing targets
          // and listen callsite may care about the full initialization
          await onAvailable({
            type: this.getTargetType(targetFront),
            targetFront,
            isTopLevel: targetFront == this.targetFront,
            isTargetSwitching: false,
          });
        } catch (e) {
          // Prevent throwing when onAvailable handler throws on one target
          // so that it can try to register the other targets
          console.error(
            "Exception when calling onAvailable handler",
            e.message,
            e
          );
        }
      });

    for (const type of types) {
      this._createListeners.on(type, onAvailable);
      if (onDestroy) {
        this._destroyListeners.on(type, onDestroy);
      }
    }

    await Promise.all(promises);
  }

  /**
   * Stop listening for the creation and/or destruction of a given type of target fronts.
   * See `watchTargets()` for documentation of the arguments.
   */
  async unwatchTargets(types, onAvailable, onDestroy) {
    if (typeof onAvailable != "function") {
      throw new Error(
        "TargetList.unwatchTargets expects a function as second argument"
      );
    }

    for (const type of types) {
      this._createListeners.off(type, onAvailable);
      if (onDestroy) {
        this._destroyListeners.off(type, onDestroy);
      }
    }
  }

  /**
   * Retrieve all the current target fronts of a given type.
   *
   * @param {String} type
   *        The type of target to retrieve. Constant of TargetList.TYPES.
   */
  getAllTargets(type) {
    if (!type) {
      throw new Error("getAllTargets expects a 'type' argument");
    }

    const targets = [...this._targets].filter(target =>
      this._matchTargetType(type, target)
    );

    return targets;
  }

  /**
   * For all the target fronts of a given type, retrieve all the target-scoped fronts of a given type.
   *
   * @param {String} targetType
   *        The type of target to iterate over. Constant of TargetList.TYPES.
   * @param {String} frontType
   *        The type of target-scoped front to retrieve. It can be "inspector", "console", "thread",...
   */
  async getAllFronts(targetType, frontType) {
    const fronts = [];
    const targets = this.getAllTargets(targetType);
    for (const target of targets) {
      const front = await target.getFront(frontType);
      fronts.push(front);
    }
    return fronts;
  }

  /**
   * Called when the top level target is replaced by a new one.
   * Typically when we navigate to another domain which requires to be loaded in a distinct process.
   *
   * @param {TargetFront} newTarget
   *        The new top level target to debug.
   */
  async switchToTarget(newTarget) {
    // First report that all existing targets are destroyed
    for (const target of this._targets) {
      // We only consider the top level target to be switched
      const isTargetSwitching = target == this.targetFront;
      this._onTargetDestroyed(target, isTargetSwitching);
    }
    this.stopListening();

    // Clear the cached target list
    this._targets.clear();

    // Update the reference to the top level target so that
    // creation listening can know this is about the top level target
    this.targetFront = newTarget;

    // Notify about this new target to creation listeners
    await this._onTargetAvailable(newTarget, true);

    // Re-register the listeners as the top level target changed
    // and some targets are fetched from it
    await this.startListening();
  }
}

/**
 * All types of target:
 */
TargetList.TYPES = TargetList.prototype.TYPES = {
  PROCESS: "process",
  FRAME: "frame",
  WORKER: "worker",
};
TargetList.ALL_TYPES = TargetList.prototype.ALL_TYPES = Object.values(
  TargetList.TYPES
);

module.exports = { TargetList };
