/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// eslint-disable-next-line mozilla/reject-some-requires
const { WorkersListener } = require("devtools/client/shared/workers-listener");

const {
  LegacyWorkersWatcher,
} = require("devtools/shared/commands/target/legacy-target-watchers/legacy-workers-watcher");

class LegacyServiceWorkersWatcher extends LegacyWorkersWatcher {
  constructor(targetCommand, onTargetAvailable, onTargetDestroyed) {
    super(targetCommand, onTargetAvailable, onTargetDestroyed);
    this._registrations = [];
    this._processTargets = new Set();

    // We need to listen for registration changes at least in order to properly
    // filter service workers by domain when debugging a local tab.
    //
    // A WorkerTarget instance has a url property, but it points to the url of
    // the script, whereas the url property of the ServiceWorkerRegistration
    // points to the URL controlled by the service worker.
    //
    // Historically we have been matching the service worker registration URL
    // to match service workers for local tab tools (app panel & debugger).
    // Maybe here we could have some more info on the actual worker.
    this._workersListener = new WorkersListener(this.rootFront, {
      registrationsOnly: true,
    });

    // Note that this is called much more often than when a registration
    // is created or destroyed. WorkersListener notifies of anything that
    // potentially impacted workers.
    // I use it as a shortcut in this first patch. Listening to rootFront's
    // "serviceWorkerRegistrationListChanged" should be enough to be notified
    // about registrations. And if we need to also update the
    // "debuggerServiceWorkerStatus" from here, then we would have to
    // also listen to "registration-changed" one each registration.
    this._onRegistrationListChanged = this._onRegistrationListChanged.bind(
      this
    );
    this._onNavigate = this._onNavigate.bind(this);

    // Flag used from the parent class to listen to process targets.
    // Decision tree is complicated, keep all logic in the parent methods.
    this._isServiceWorkerWatcher = true;
  }

  /**
   * Override from LegacyWorkersWatcher.
   *
   * We record all valid service worker targets (ie workers that match a service
   * worker registration), but we will only notify about the ones which match
   * the current domain.
   */
  _recordWorkerTarget(workerTarget) {
    return !!this._getRegistrationForWorkerTarget(workerTarget);
  }

  // Override from LegacyWorkersWatcher.
  _supportWorkerTarget(workerTarget) {
    if (!workerTarget.isServiceWorker) {
      return false;
    }

    const registration = this._getRegistrationForWorkerTarget(workerTarget);
    return registration && this._isRegistrationValidForTarget(registration);
  }

  // Override from LegacyWorkersWatcher.
  async listen() {
    // Listen to the current target front.
    this.target = this.targetCommand.targetFront;

    this._workersListener.addListener(this._onRegistrationListChanged);

    // Fetch the registrations before calling listen, since service workers
    // might already be available and will need to be compared with the existing
    // registrations.
    await this._onRegistrationListChanged();

    if (this.targetCommand.descriptorFront.isLocalTab) {
      // Note that we rely on "navigate" rather than "will-navigate" because the
      // destroyed/available callbacks should be triggered after the Debugger
      // has cleaned up its reducers, which happens on "will-navigate".
      this.target.on("navigate", this._onNavigate);
    }

    await super.listen();
  }

  // Override from LegacyWorkersWatcher.
  unlisten() {
    this._workersListener.removeListener(this._onRegistrationListChanged);

    if (this.targetCommand.descriptorFront.isLocalTab) {
      this.target.off("navigate", this._onNavigate);
    }

    super.unlisten();
  }

  // Override from LegacyWorkersWatcher.
  async _onProcessAvailable({ targetFront }) {
    if (this.targetCommand.descriptorFront.isLocalTab) {
      // XXX: This has been ported straight from the current debugger
      // implementation. Since pauseMatchingServiceWorkers expects an origin
      // to filter matching workers, it only makes sense when we are debugging
      // a tab. However in theory, parent process debugging could pause all
      // service workers without matching anything.
      const origin = new URL(this.target.url).origin;
      try {
        // To support early breakpoint we need to setup the
        // `pauseMatchingServiceWorkers` mechanism in each process.
        await targetFront.pauseMatchingServiceWorkers({ origin });
      } catch (e) {
        if (targetFront.actorID) {
          throw e;
        } else {
          console.warn(
            "Process target destroyed while calling pauseMatchingServiceWorkers"
          );
        }
      }
    }

    this._processTargets.add(targetFront);
    return super._onProcessAvailable({ targetFront });
  }

  _shouldDestroyTargetsOnNavigation() {
    return !!this.targetCommand.destroyServiceWorkersOnNavigation;
  }

  _onProcessDestroyed({ targetFront }) {
    this._processTargets.delete(targetFront);
    return super._onProcessDestroyed({ targetFront });
  }

  _onNavigate() {
    const allServiceWorkerTargets = this._getAllServiceWorkerTargets();
    const shouldDestroy = this._shouldDestroyTargetsOnNavigation();

    for (const target of allServiceWorkerTargets) {
      const isRegisteredBefore = this.targetCommand.isTargetRegistered(target);
      if (shouldDestroy && isRegisteredBefore) {
        // Instruct the target command to notify about the worker target destruction
        // but do not destroy the front as we want to keep using it.
        // We will notify about it again via onTargetAvailable.
        this.onTargetDestroyed(target, { shouldDestroyTargetFront: false });
      }

      // Note: we call isTargetRegistered again because calls to
      // onTargetDestroyed might have modified the list of registered targets.
      const isRegisteredAfter = this.targetCommand.isTargetRegistered(target);
      const isValidTarget = this._supportWorkerTarget(target);
      if (isValidTarget && !isRegisteredAfter) {
        // If the target is still valid for the current top target, call
        // onTargetAvailable as well.
        this.onTargetAvailable(target);
      }
    }
  }

  async _onRegistrationListChanged() {
    if (this.targetCommand.isDestroyed()) {
      return;
    }

    await this._updateRegistrations();

    // Everything after this point is not strictly necessary for sw support
    // in the target list, but it makes the behavior closer to the previous
    // listAllWorkers/WorkersListener pair.
    const allServiceWorkerTargets = this._getAllServiceWorkerTargets();
    for (const target of allServiceWorkerTargets) {
      const hasRegistration = this._getRegistrationForWorkerTarget(target);
      if (!hasRegistration) {
        // XXX: At this point the worker target is not really destroyed, but
        // historically, listAllWorkers* APIs stopped returning worker targets
        // if worker registrations are no longer available.
        if (this.targetCommand.isTargetRegistered(target)) {
          // Only emit onTargetDestroyed if it wasn't already done by
          // onNavigate (ie the target is still tracked by TargetCommand)
          this.onTargetDestroyed(target);
        }
        // Here we only care about service workers which no longer match *any*
        // registration. The worker will be completely destroyed soon, remove
        // it from the legacy worker watcher internal targetsByProcess Maps.
        this._removeTargetReferences(target);
      }
    }
  }

  // Delete the provided worker target from the internal targetsByProcess Maps.
  _removeTargetReferences(target) {
    const allProcessTargets = this._getProcessTargets().filter(t =>
      this.targetsByProcess.get(t)
    );

    for (const processTarget of allProcessTargets) {
      this.targetsByProcess.get(processTarget).delete(target);
    }
  }

  async _updateRegistrations() {
    const {
      registrations,
    } = await this.rootFront.listServiceWorkerRegistrations();

    this._registrations = registrations;
  }

  _getRegistrationForWorkerTarget(workerTarget) {
    return this._registrations.find(r => {
      return (
        r.evaluatingWorker?.id === workerTarget.id ||
        r.activeWorker?.id === workerTarget.id ||
        r.installingWorker?.id === workerTarget.id ||
        r.waitingWorker?.id === workerTarget.id
      );
    });
  }

  _getProcessTargets() {
    return [...this._processTargets];
  }

  // Flatten all service worker targets in all processes.
  _getAllServiceWorkerTargets() {
    const allProcessTargets = this._getProcessTargets().filter(target =>
      this.targetsByProcess.get(target)
    );

    const serviceWorkerTargets = [];
    for (const target of allProcessTargets) {
      serviceWorkerTargets.push(...this.targetsByProcess.get(target));
    }
    return serviceWorkerTargets;
  }

  // Check if the registration is relevant for the current target, ie
  // corresponds to the same domain.
  _isRegistrationValidForTarget(registration) {
    if (this.target.isParentProcess) {
      // All registrations are valid for main process debugging.
      return true;
    }

    if (!this.targetCommand.descriptorFront.isLocalTab) {
      // No support for service worker targets outside of main process & local
      // tab debugging.
      return false;
    }

    // For local tabs, we match ServiceWorkerRegistrations and the target
    // if they share the same hostname for their "url" properties.
    const targetDomain = new URL(this.target.url).hostname;
    try {
      const registrationDomain = new URL(registration.url).hostname;
      return registrationDomain === targetDomain;
    } catch (e) {
      // XXX: Some registrations have an empty URL.
      return false;
    }
  }
}

module.exports = { LegacyServiceWorkersWatcher };
