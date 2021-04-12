/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const EventEmitter = require("devtools/shared/event-emitter");

const BROWSERTOOLBOX_FISSION_ENABLED = "devtools.browsertoolbox.fission";
const SERVER_TARGET_SWITCHING_ENABLED =
  "devtools.target-switching.server.enabled";

const {
  LegacyProcessesWatcher,
} = require("devtools/shared/commands/target/legacy-target-watchers/legacy-processes-watcher");
const {
  LegacyServiceWorkersWatcher,
} = require("devtools/shared/commands/target/legacy-target-watchers/legacy-serviceworkers-watcher");
const {
  LegacySharedWorkersWatcher,
} = require("devtools/shared/commands/target/legacy-target-watchers/legacy-sharedworkers-watcher");
const {
  LegacyWorkersWatcher,
} = require("devtools/shared/commands/target/legacy-target-watchers/legacy-workers-watcher");

class TargetCommand extends EventEmitter {
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
   * @fires target-tread-wrong-order-on-resume : An event that is emitted when resuming
   *        the thread throws with the "wrongOrder" error.
   *
   * @param {DescriptorFront} descriptorFront
   *        The context to inspector identified by this descriptor.
   */
  constructor({ descriptorFront }) {
    super();

    this.descriptorFront = descriptorFront;
    this.rootFront = descriptorFront.client.mainRoot;

    // Until Watcher actor notify about new top level target when navigating to another process
    // we have to manually switch to a new target from the client side
    this.onLocalTabRemotenessChange = this.onLocalTabRemotenessChange.bind(
      this
    );
    if (this.descriptorFront.isLocalTab) {
      this.descriptorFront.on(
        "remoteness-change",
        this.onLocalTabRemotenessChange
      );
    }

    // Reports if we have at least one listener for the given target type
    this._listenersStarted = new Set();

    // List of all the target fronts
    this._targets = new Set();
    // {Map<Function, Set<targetFront>>} A Map keyed by `onAvailable` function passed to
    // `watchTargets`, whose initial value is a Set of the existing target fronts at the
    // time watchTargets is called.
    this._pendingWatchTargetInitialization = new Map();

    // Listeners for target creation and destruction
    this._createListeners = new EventEmitter();
    this._destroyListeners = new EventEmitter();

    this._onTargetAvailable = this._onTargetAvailable.bind(this);
    this._onTargetDestroyed = this._onTargetDestroyed.bind(this);

    this.legacyImplementation = {
      process: new LegacyProcessesWatcher(
        this,
        this._onTargetAvailable,
        this._onTargetDestroyed
      ),
      worker: new LegacyWorkersWatcher(
        this,
        this._onTargetAvailable,
        this._onTargetDestroyed
      ),
      shared_worker: new LegacySharedWorkersWatcher(
        this,
        this._onTargetAvailable,
        this._onTargetDestroyed
      ),
      service_worker: new LegacyServiceWorkersWatcher(
        this,
        this._onTargetAvailable,
        this._onTargetDestroyed
      ),
    };

    // Public flag to allow listening for workers even if the fission pref is off
    // This allows listening for workers in the content toolbox outside of fission contexts
    // For now, this is only toggled by tests.
    this.listenForWorkers =
      this.rootFront.traits.workerConsoleApiMessagesDispatchedToMainThread ===
      false;
    this.listenForServiceWorkers = false;
    this.destroyServiceWorkersOnNavigation = false;
  }

  // Called whenever a new Target front is available.
  // Either because a target was already available as we started calling startListening
  // or if it has just been created
  async _onTargetAvailable(targetFront) {
    // If the new target is a top level target, we are target switching.
    // Target-switching is only triggered for "local-tab" browsing-context
    // targets which should always have the topLevelTarget flag initialized
    // on the server.
    const isTargetSwitching = targetFront.isTopLevel;

    if (this._targets.has(targetFront)) {
      // The top level target front can be reported via listProcesses in the
      // case of the BrowserToolbox. For any other target, log an error if it is
      // already registered.
      if (targetFront != this.targetFront) {
        console.error(
          "Target is already registered in the TargetCommand",
          targetFront.actorID
        );
      }
      return;
    }

    if (this.isDestroyed() || targetFront.isDestroyedOrBeingDestroyed()) {
      return;
    }

    // Handle top level target switching
    // Note that, for now, `_onTargetAvailable` isn't called for the *initial* top level target.
    // i.e. the one that is passed to TargetCommand constructor.
    if (targetFront.isTopLevel) {
      // First report that all existing targets are destroyed
      for (const target of this._targets) {
        // We only consider the top level target to be switched
        const isDestroyedTargetSwitching = target == this.targetFront;
        this._onTargetDestroyed(target, {
          isTargetSwitching: isDestroyedTargetSwitching,
        });
      }
      // Stop listening to legacy listeners as we now have to listen
      // on the new target.
      this.stopListening({ onlyLegacy: true });

      // Clear the cached target list
      this._targets.clear();

      // Update the reference to the memoized top level target
      this.targetFront = targetFront;
      this.descriptorFront.setTarget(targetFront);
    }

    // Map the descriptor typeName to a target type.
    const targetType = this.getTargetType(targetFront);
    targetFront.setTargetType(targetType);

    this._targets.add(targetFront);
    try {
      await targetFront.attachAndInitThread(this);
    } catch (e) {
      console.error("Error when attaching target:", e);
      this._targets.delete(targetFront);
      return;
    }

    for (const targetFrontsSet of this._pendingWatchTargetInitialization.values()) {
      targetFrontsSet.delete(targetFront);
    }

    if (this.isDestroyed() || targetFront.isDestroyedOrBeingDestroyed()) {
      return;
    }

    // Then, once the target is attached, notify the target front creation listeners
    await this._createListeners.emitAsync(targetType, {
      targetFront,
      isTargetSwitching,
    });

    // Re-register the listeners as the top level target changed
    // and some targets are fetched from it
    if (targetFront.isTopLevel) {
      await this.startListening({ onlyLegacy: true });
    }

    // To be consumed by tests triggering frame navigations, spawning workers...
    this.emitForTests("processed-available-target", targetFront);
    if (isTargetSwitching) {
      this.emitForTests("switched-target", targetFront);
    }
  }

  /**
   * Function fired everytime a target is destroyed.
   *
   * This is called either:
   * - via target-destroyed event fired by the WatcherFront,
   *   event which is a simple translation of the target-destroyed-form emitted by the WatcherActor.
   *   Watcher Actor emits this is various condition when the debugged target is meant to be destroyed:
   *   - the related target context is destroyed (tab closed, worker shut down, content process destroyed, ...),
   *   - when the DevToolsServerConnection used on the server side to communicate to the client is closed.

   * - by TargetCommand._onTargetAvailable, when a top level target switching happens and all previously
   *   registered target fronts should be destroyed.

   * - by the legacy Targets listeners, calling this method directly.
   *   This usecase is meant to be removed someday when all target targets are supported by the Watcher.
   *   (bug 1687459)
   *
   * @param {TargetFront} targetFront
   *        The target that just got destroyed.
   * @param Object options
   *        Dictionary object with:
   *        - `isTargetSwitching` optional boolean. To be set to true when this
   *           is about the top level target which is being replaced by a new one.
   *           The passed target should be still the one store in TargetCommand.targetFront
   *           and will be replaced via a call to onTargetAvailable with a new target front.
   *        - `shouldDestroyTargetFront` optional boolean. By default, the passed target
   *           front will be destroyed. But in some cases like legacy listeners for service workers
   *           we want to keep the front alive.
   */
  _onTargetDestroyed(
    targetFront,
    { isTargetSwitching = false, shouldDestroyTargetFront = true } = {}
  ) {
    // The watcher actor may notify us about the destruction of the top level target.
    // But second argument to this method, isTargetSwitching is only passed from the frontend.
    // So automatically toggle the isTargetSwitching flag for server side destructions
    // only if that's about the existing top level target.
    if (targetFront == this.targetFront) {
      isTargetSwitching = true;
    }
    this._destroyListeners.emit(targetFront.targetType, {
      targetFront,
      isTargetSwitching,
    });
    this._targets.delete(targetFront);

    if (shouldDestroyTargetFront) {
      targetFront.destroy();
    }
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

  hasTargetWatcherSupport(type) {
    // If the top level target is a parent process, we're in the browser console or browser toolbox.
    // In such case, if the browser toolbox fission pref is disabled, we don't want to use watchers
    // (even if traits on the server are enabled).
    if (
      this.targetFront.isParentProcess &&
      !Services.prefs.getBoolPref(BROWSERTOOLBOX_FISSION_ENABLED, false)
    ) {
      return false;
    }

    return !!this.watcherFront?.traits[type];
  }

  isServerTargetSwitchingEnabled() {
    if (typeof this._isServerTargetSwitchingEnabled == "undefined") {
      this._isServerTargetSwitchingEnabled = Services.prefs.getBoolPref(
        SERVER_TARGET_SWITCHING_ENABLED,
        false
      );
    }
    return this._isServerTargetSwitchingEnabled;
  }

  /**
   * Start listening for targets from the server
   *
   * Interact with the actors in order to start listening for new types of targets.
   * This will fire the _onTargetAvailable function for all already-existing targets,
   * as well as the next one to be created. It will also call _onTargetDestroyed
   * everytime a target is reported as destroyed by the actors.
   * By the time this function resolves, all the already-existing targets will be
   * reported to _onTargetAvailable.
   *
   * @param Object options
   *        Dictionary object with `onlyLegacy` optional boolean.
   *        If true, we wouldn't register listener set on the Watcher Actor,
   *        but still register listeners set via Legacy Listeners.
   */
  async startListening({ onlyLegacy = false } = {}) {
    // The first time we call this method, we pull the current top level target from the descriptor
    if (!this.targetFront) {
      // Note that this is a public attribute, used outside of this class
      // and helps knowing what is the current top level target we debug.
      this.targetFront = await this.descriptorFront.getTarget();
      this.targetFront.setTargetType(this.getTargetType(this.targetFront));
      this.targetFront.setIsTopLevel(true);

      // Add the top-level target to the list of targets.
      this._targets.add(this.targetFront);
    }

    // Cache the Watcher once for all, the first time we call `startListening()`.
    // This `watcherFront` attribute may be then used in any function in TargetCommand or ResourceWatcher after this.
    if (!this.watcherFront) {
      // Bug 1675763: Watcher actor is not available in all situations yet.
      const supportsWatcher = this.descriptorFront.traits?.watcher;
      if (supportsWatcher) {
        this.watcherFront = await this.descriptorFront.getWatcher();
        this.watcherFront.on("target-available", this._onTargetAvailable);
        this.watcherFront.on("target-destroyed", this._onTargetDestroyed);
      }
    }

    let types = [];
    if (this.targetFront.isParentProcess) {
      const fissionBrowserToolboxEnabled = Services.prefs.getBoolPref(
        BROWSERTOOLBOX_FISSION_ENABLED
      );
      if (fissionBrowserToolboxEnabled) {
        types = TargetCommand.ALL_TYPES;
      }
    } else if (this.descriptorFront.isLocalTab) {
      types = [TargetCommand.TYPES.FRAME];
    }
    if (this.listenForWorkers && !types.includes(TargetCommand.TYPES.WORKER)) {
      types.push(TargetCommand.TYPES.WORKER);
    }
    if (
      this.listenForWorkers &&
      !types.includes(TargetCommand.TYPES.SHARED_WORKER)
    ) {
      types.push(TargetCommand.TYPES.SHARED_WORKER);
    }
    if (
      this.listenForServiceWorkers &&
      !types.includes(TargetCommand.TYPES.SERVICE_WORKER)
    ) {
      types.push(TargetCommand.TYPES.SERVICE_WORKER);
    }

    // If no pref are set to true, nor is listenForWorkers set to true,
    // we won't listen for any additional target. Only the top level target
    // will be managed. We may still do target-switching.

    for (const type of types) {
      if (this._isListening(type)) {
        continue;
      }
      this._setListening(type, true);

      // Only a few top level targets support the watcher actor at the moment (see WatcherActor
      // traits in the _form method). Bug 1675763 tracks watcher actor support for all targets.
      if (this.hasTargetWatcherSupport(type)) {
        // When we switch to a new top level target, we don't have to stop and restart
        // Watcher listener as it is independant from the top level target.
        // This isn't the case for some Legacy Listeners, which fetch targets from the top level target
        if (!onlyLegacy) {
          await this.watcherFront.watchTargets(type);
        }
      } else if (this.legacyImplementation[type]) {
        await this.legacyImplementation[type].listen();
      } else {
        throw new Error(`Unsupported target type '${type}'`);
      }
    }
  }

  /**
   * Stop listening for targets from the server
   *
   * @param Object options
   *        Dictionary object with `onlyLegacy` optional boolean.
   *        If true, we wouldn't unregister listener set on the Watcher Actor,
   *        but still unregister listeners set via Legacy Listeners.
   */
  stopListening({ onlyLegacy = false } = {}) {
    for (const type of TargetCommand.ALL_TYPES) {
      if (!this._isListening(type)) {
        continue;
      }
      this._setListening(type, false);

      // Only a few top level targets support the watcher actor at the moment (see WatcherActor
      // traits in the _form method). Bug 1675763 tracks watcher actor support for all targets.
      if (this.hasTargetWatcherSupport(type)) {
        // When we switch to a new top level target, we don't have to stop and restart
        // Watcher listener as it is independant from the top level target.
        // This isn't the case for some Legacy Listeners, which fetch targets from the top level target
        if (!onlyLegacy) {
          this.watcherFront.unwatchTargets(type);
        }
      } else if (this.legacyImplementation[type]) {
        this.legacyImplementation[type].unlisten();
      } else {
        throw new Error(`Unsupported target type '${type}'`);
      }
    }
  }

  getTargetType(target) {
    const { typeName } = target;
    if (typeName == "browsingContextTarget") {
      return TargetCommand.TYPES.FRAME;
    }

    if (
      typeName == "contentProcessTarget" ||
      typeName == "parentProcessTarget"
    ) {
      return TargetCommand.TYPES.PROCESS;
    }

    if (typeName == "workerDescriptor" || typeName == "workerTarget") {
      if (target.isSharedWorker) {
        return TargetCommand.TYPES.SHARED_WORKER;
      }

      if (target.isServiceWorker) {
        return TargetCommand.TYPES.SERVICE_WORKER;
      }

      return TargetCommand.TYPES.WORKER;
    }

    throw new Error("Unsupported target typeName: " + typeName);
  }

  _matchTargetType(type, target) {
    return type === target.targetType;
  }

  /**
   * Listen for the creation and/or destruction of target fronts matching one of the provided types.
   *
   * @param {Array<String>} types
   *        The type of target to listen for. Constant of TargetCommand.TYPES.
   * @param {Function} onAvailable
   *        Callback fired when a target has been just created or was already available.
   *        The function is called with the following arguments:
   *        - {TargetFront} targetFront: The target Front
   *        - {Boolean} isTargetSwitching: Is this target relates to a navigation and
   *                    this replaced a previously available target, this flag will be true
   * @param {Function} onDestroy
   *        Callback fired in case of target front destruction.
   *        The function is called with the same arguments than onAvailable.
   */
  async watchTargets(types, onAvailable, onDestroy) {
    if (typeof onAvailable != "function") {
      throw new Error(
        "TargetCommand.watchTargets expects a function as second argument"
      );
    }

    // Notify about already existing target of these types
    const targetFronts = [...this._targets].filter(targetFront =>
      types.includes(targetFront.targetType)
    );
    this._pendingWatchTargetInitialization.set(
      onAvailable,
      new Set(targetFronts)
    );
    const promises = targetFronts.map(async targetFront => {
      // Attach the targets that aren't attached yet (e.g. the initial top-level target),
      // and wait for the other ones to be fully attached.
      try {
        await targetFront.attachAndInitThread(this);
      } catch (e) {
        console.error("Error when attaching target:", e);
        return;
      }

      // It can happen that onAvailable was already called with this targetFront at
      // this time (via _onTargetAvailable). If that's the case, we don't want to call
      // onAvailable a second time.
      if (
        this._pendingWatchTargetInitialization &&
        this._pendingWatchTargetInitialization.has(onAvailable) &&
        !this._pendingWatchTargetInitialization
          .get(onAvailable)
          .has(targetFront)
      ) {
        return;
      }

      try {
        // Ensure waiting for eventual async create listeners
        // which may setup things regarding the existing targets
        // and listen callsite may care about the full initialization
        await onAvailable({
          targetFront,
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
    this._pendingWatchTargetInitialization.delete(onAvailable);
  }

  /**
   * Stop listening for the creation and/or destruction of a given type of target fronts.
   * See `watchTargets()` for documentation of the arguments.
   */
  unwatchTargets(types, onAvailable, onDestroy) {
    if (typeof onAvailable != "function") {
      throw new Error(
        "TargetCommand.unwatchTargets expects a function as second argument"
      );
    }

    for (const type of types) {
      this._createListeners.off(type, onAvailable);
      if (onDestroy) {
        this._destroyListeners.off(type, onDestroy);
      }
    }
    this._pendingWatchTargetInitialization.delete(onAvailable);
  }

  /**
   * Retrieve all the current target fronts of a given type.
   *
   * @param {Array<String>} types
   *        The types of target to retrieve. Array of TargetCommand.TYPES
   * @return {Array<TargetFront>} Array of target fronts matching any of the
   *         provided types.
   */
  getAllTargets(types) {
    if (!types?.length) {
      throw new Error("getAllTargets expects a non-empty array of types");
    }

    const targets = [...this._targets].filter(target =>
      types.some(type => this._matchTargetType(type, target))
    );

    return targets;
  }

  /**
   * For all the target fronts of a given type, retrieve all the target-scoped fronts of a given type.
   *
   * @param {String} targetType
   *        The type of target to iterate over. Constant of TargetCommand.TYPES.
   * @param {String} frontType
   *        The type of target-scoped front to retrieve. It can be "inspector", "console", "thread",...
   */
  async getAllFronts(targetType, frontType) {
    const fronts = [];
    const targets = this.getAllTargets([targetType]);
    for (const target of targets) {
      const front = await target.getFront(frontType);
      fronts.push(front);
    }
    return fronts;
  }

  /**
   * This function is triggered by an event sent by the TabDescriptor when
   * the tab navigates to a distinct process.
   *
   * @param TargetFront targetFront
   *        The BrowsingContextTargetFront instance that navigated to another process
   */
  async onLocalTabRemotenessChange(targetFront) {
    if (this.isServerTargetSwitchingEnabled()) {
      // For server-side target switchting, everything will be handled by the
      // _onTargetAvailable callback.
      return;
    }

    // TabDescriptor may emit the event with a null targetFront, interpret that as if the previous target
    // has already been destroyed
    if (targetFront) {
      // Wait for the target to be destroyed so that TabDescriptorFactory clears its memoized target for this tab
      await targetFront.once("target-destroyed");
    }

    // Fetch the new target from the descriptor.
    const newTarget = await this.descriptorFront.getTarget();

    this.switchToTarget(newTarget);
  }

  /**
   * Called when the top level target is replaced by a new one.
   * Typically when we navigate to another domain which requires to be loaded in a distinct process.
   *
   * @param {TargetFront} newTarget
   *        The new top level target to debug.
   */
  async switchToTarget(newTarget) {
    // Notify about this new target to creation listeners
    // _onTargetAvailable will also destroy all previous target before notifying about this new one.
    await this._onTargetAvailable(newTarget);
  }

  isTargetRegistered(targetFront) {
    return this._targets.has(targetFront);
  }

  isDestroyed() {
    return this._isDestroyed;
  }

  destroy() {
    this.stopListening();
    this._createListeners.off();
    this._destroyListeners.off();
    this._isDestroyed = true;
  }
}

/**
 * All types of target:
 */
TargetCommand.TYPES = TargetCommand.prototype.TYPES = {
  PROCESS: "process",
  FRAME: "frame",
  WORKER: "worker",
  SHARED_WORKER: "shared_worker",
  SERVICE_WORKER: "service_worker",
};
TargetCommand.ALL_TYPES = TargetCommand.prototype.ALL_TYPES = Object.values(
  TargetCommand.TYPES
);

module.exports = TargetCommand;
