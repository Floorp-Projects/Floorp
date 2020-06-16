/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// eslint-disable-next-line mozilla/reject-some-requires
const { WorkersListener } = require("devtools/client/shared/workers-listener");

const {
  LegacyWorkersWatcher,
} = require("devtools/shared/resources/legacy-target-watchers/legacy-workers-watcher");

class LegacyServiceWorkersWatcher extends LegacyWorkersWatcher {
  constructor(...args) {
    super(...args);
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

    // Flag used from the parent class to listen to process targets.
    // Decision tree is complicated, keep all logic in the parent methods.
    this._isServiceWorkerWatcher = true;
  }

  // Override from LegacyWorkersWatcher.
  _supportWorkerTarget(workerTarget) {
    if (!workerTarget.isServiceWorker) {
      return false;
    }

    const swFronts = this._getAllServiceWorkerFronts();
    return swFronts.some(({ id }) => id === workerTarget.id);
  }

  // Override from LegacyWorkersWatcher.
  async listen() {
    this._workersListener.addListener(this._onRegistrationListChanged);

    // Fetch the registrations before calling listen, since service workers
    // might already be available and will need to be compared with the existing
    // registrations.
    await this._onRegistrationListChanged();

    await super.listen();
  }

  // Override from LegacyWorkersWatcher.
  unlisten() {
    this._workersListener.removeListener(this._onRegistrationListChanged);

    super.unlisten();
  }

  // Override from LegacyWorkersWatcher.
  async _onProcessAvailable({ targetFront }) {
    if (this.target.isLocalTab) {
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

  _onProcessDestroyed({ targetFront }) {
    this._processTargets.delete(targetFront);
    return super._onProcessDestroyed({ targetFront });
  }

  async _onRegistrationListChanged() {
    const {
      registrations,
    } = await this.rootFront.listServiceWorkerRegistrations();

    this._registrations = registrations.filter(r =>
      this._isRegistrationValid(r)
    );

    // Everything after this point is not strictly necessary for sw support
    // in the target list, but it makes the behavior closer to the previous
    // listAllWorkers/WorkersListener pair.
    const allServiceWorkerTargets = this._getAllServiceWorkerTargets();
    const swFronts = this._getAllServiceWorkerFronts();
    for (const target of allServiceWorkerTargets) {
      const match = swFronts.find(({ id }) => id === target.id);
      if (!match) {
        // XXX: At this point the worker target is not really destroyed, but
        // historically, listAllWorkers* APIs stopped returning worker targets
        // if worker registrations are no longer available.
        this.onTargetDestroyed(target);
        this._removeTargetReferences(target);
      }
    }
  }

  // Retrieve all the ServiceWorkerFronts currently known.
  _getAllServiceWorkerFronts() {
    return (
      this._registrations
        // Flatten all ServiceWorkerRegistration fronts into list of
        // ServiceWorker fronts. ServiceWorker fronts are just a description
        // class and are not targets. They are not WorkerTarget fronts.
        .reduce((p, registration) => {
          return [
            registration.evaluatingWorker,
            registration.activeWorker,
            registration.installingWorker,
            registration.waitingWorker,
            ...p,
          ];
        }, [])
        // Filter out null workers, most registrations only have one worker
        // set at a given time.
        .filter(Boolean)
    );
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

  // Delete the provided worker target from the internal targetsByProcess Maps.
  _removeTargetReferences(target) {
    const allProcessTargets = this._getProcessTargets().filter(t =>
      this.targetsByProcess.get(t)
    );

    for (const processTarget of allProcessTargets) {
      this.targetsByProcess.get(processTarget).delete(target);
    }
  }

  // Check if the registration is relevant for the current target, ie
  // corresponds to the same domain.
  _isRegistrationValid(registration) {
    if (this.target.isParentProcess) {
      // All registrations are valid for main process debugging.
      return true;
    }

    if (!this.target.isLocalTab) {
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
