/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  frameDescriptorSpec,
} = require("devtools/shared/specs/descriptors/frame");

loader.lazyRequireGetter(
  this,
  "BrowsingContextTargetFront",
  "devtools/client/fronts/targets/browsing-context",
  true
);

const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");

class FrameDescriptorFront extends FrontClassWithSpec(frameDescriptorSpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);
    this._frameDescriptorFront = null;
    this._targetFrontPromise = null;
    this._client = client;
  }

  form(json) {
    this.id = json.id;
    this.url = json.url;
    this.parentID = json.parentID;
  }

  async _createFrameTarget(form) {
    let front = null;
    front = new BrowsingContextTargetFront(this._client, null, this);
    front.actorID = form.actor;
    front.form(form);
    this.manage(front);
    return front;
  }

  async getTarget() {
    // Only return the cached Target if it is still alive.
    // It may have been destroyed in case of navigation trigerring a load in another
    // process.
    if (this._frameTargetFront && this._frameTargetFront.actorID) {
      return this._frameTargetFront;
    }
    // Otherwise, ensure that we don't try to spawn more than one Target by
    // returning the pending promise
    if (this._targetFrontPromise) {
      return this._targetFrontPromise;
    }
    this._targetFrontPromise = (async () => {
      let target = null;
      try {
        const targetForm = await super.getTarget();
        // getTarget uses 'json' in the specification type and this prevents converting
        // actor exception into front exceptions
        if (targetForm.error) {
          throw new Error(targetForm.error);
        }
        target = await this._createFrameTarget(targetForm);
        await target.attach();
      } catch (e) {
        // This is likely to happen if we get a lot of events which drop previous
        // frames.
        console.log(
          `Request to connect to frameDescriptor "${this.id}" failed: ${e}`
        );
      }
      // Save the reference to the target only after the call to attach
      // so that getTarget always returns the attached target in case of concurrent calls
      this._frameTargetFront = target;
      // clear the promise if we are finished so that we can re-connect if
      // necessary
      this._targetFrontPromise = null;
      return target;
    })();
    return this._targetFrontPromise;
  }

  async getParentTarget() {
    if (!this.parentID) {
      return null;
    }
    const parentDescriptor = await this._client.mainRoot.getBrowsingContextDescriptor(
      this.parentID
    );
    return parentDescriptor.getTarget();
  }

  getCachedWatcher() {
    for (const child of this.poolChildren()) {
      if (child.typeName == "watcher") {
        return child;
      }
    }
    return null;
  }

  destroy() {
    this._frameTargetFront = null;
    this._targetFrontPromise = null;
    super.destroy();
  }
}

exports.FrameDescriptorFront = FrameDescriptorFront;
registerFront(FrameDescriptorFront);
