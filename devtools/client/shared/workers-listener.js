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

    // bind handlers
    this._onContentProcessTargetAvailable = this._onContentProcessTargetAvailable.bind(
      this
    );
    this._onServiceWorkerRegistrationAvailable = this._onServiceWorkerRegistrationAvailable.bind(
      this
    );
    this._onProcessDescriptorAvailable = this._onProcessDescriptorAvailable.bind(
      this
    );

    // Array of contentProcessTarget fronts on which we will listen for worker events.
    this._contentProcessFronts = [];
    this._serviceWorkerRegistrationFronts = [];
    this._processDescriptors = [];
    this._listener = null;
  }

  addListener(listener) {
    if (this._listener) {
      throw new Error("WorkersListener addListener called twice.");
    }

    this._listener = listener;
    this.rootFront.on("workerListChanged", this._listener);
    this.rootFront.watchFronts(
      "processDescriptor",
      this._onProcessDescriptorAvailable
    );

    // Support FF69 and older
    this.rootFront.watchFronts(
      "contentProcessTarget",
      this._onContentProcessTargetAvailable
    );

    this.rootFront.watchFronts(
      "serviceWorkerRegistration",
      this._onServiceWorkerRegistrationAvailable
    );

    this.rootFront.on("serviceWorkerRegistrationListChanged", this._listener);
    this.rootFront.on("processListChanged", this._listener);
  }

  removeListener(listener) {
    if (!this._listener) {
      return;
    }

    this.rootFront.off("workerListChanged", this._listener);
    this.rootFront.off("serviceWorkerRegistrationListChanged", this._listener);
    this.rootFront.off("processListChanged", this._listener);

    for (const front of this._contentProcessFronts) {
      front.off("workerListChanged", this._listener);
    }

    for (const front of this._serviceWorkerRegistrationFronts) {
      front.off("push-subscription-modified", this._listener);
      front.off("registration-changed", this._listener);
    }

    for (const processFront of this._processDescriptors) {
      processFront.unwatchFronts(
        "contentProcessTarget",
        this._onContentProcessTargetAvailable
      );
    }

    this.rootFront.unwatchFronts(
      "processDescriptor",
      this._onProcessDescriptorAvailable
    );
    this.rootFront.unwatchFronts(
      "contentProcessTarget",
      this._onContentProcessTargetAvailable
    );
    this.rootFront.unwatchFronts(
      "serviceWorkerRegistration",
      this._onServiceWorkerRegistrationAvailable
    );

    this._contentProcessFronts = [];
    this._serviceWorkerRegistrationFronts = [];
    this._processDescriptors = [];
    this._listener = null;
  }

  _onContentProcessTargetAvailable(front) {
    this._contentProcessFronts.push(front);
    front.on("workerListChanged", this._listener);
  }

  _onServiceWorkerRegistrationAvailable(front) {
    this._serviceWorkerRegistrationFronts.push(front);
    front.on("push-subscription-modified", this._listener);
    front.on("registration-changed", this._listener);
  }

  _onProcessDescriptorAvailable(processFront) {
    this._processDescriptors.push(processFront);
    processFront.watchFronts(
      "contentProcessTarget",
      this._onContentProcessTargetAvailable
    );
  }
}

exports.WorkersListener = WorkersListener;
