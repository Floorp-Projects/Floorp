/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Listening to worker updates requires watching various sources. This class provides
 * a single addListener/removeListener API that will aggregate all possible worker update
 * events.
 *
 * Only supports one listener at a time.
 */
class WorkersListener {
  constructor(rootFront) {
    this.rootFront = rootFront;

    // Array of contentProcessTarget fronts on which we will listen for worker events.
    this._contentProcessFronts = [];
    this._serviceWorkerRegistrationFronts = [];
    this._listener = null;
  }

  addListener(listener) {
    if (this._listener) {
      throw new Error("WorkersListener addListener called twice.");
    }

    this._listener = listener;
    this.rootFront.on("workerListChanged", this._listener);
    this.rootFront.onFront("processDescriptor", processFront => {
      processFront.onFront("contentProcessTarget", front => {
        this._contentProcessFronts.push(front);
        front.on("workerListChanged", this._listener);
      });
    });

    // Support FF69 and older
    this.rootFront.onFront("contentProcessTarget", front => {
      this._contentProcessFronts.push(front);
      front.on("workerListChanged", this._listener);
    });

    this.rootFront.onFront("serviceWorkerRegistration", front => {
      this._serviceWorkerRegistrationFronts.push(front);
      front.on("push-subscription-modified", this._listener);
      front.on("registration-changed", this._listener);
    });

    this.rootFront.on("serviceWorkerRegistrationListChanged", this._listener);
    this.rootFront.on("processListChanged", this._listener);
  }

  removeListener(listener) {
    if (!this._listener) {
      return;
    }

    this.rootFront.off("workerListChanged", this._listener);

    for (const front of this._contentProcessFronts) {
      front.off("workerListChanged", this._listener);
    }

    for (const front of this._serviceWorkerRegistrationFronts) {
      front.off("push-subscription-modified", this._listener);
      front.off("registration-changed", this._listener);
    }

    this.rootFront.off("serviceWorkerRegistrationListChanged", this._listener);
    this.rootFront.off("processListChanged", this._listener);

    this._contentProcessFronts = [];
    this._serviceWorkerRegistrationFronts = [];
    this._listener = null;
  }
}
exports.WorkersListener = WorkersListener;
