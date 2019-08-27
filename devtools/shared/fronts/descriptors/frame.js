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
  "devtools/shared/fronts/targets/browsing-context",
  true
);

const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");

class FrameDescriptorFront extends FrontClassWithSpec(frameDescriptorSpec) {
  constructor(client) {
    super(client);
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
    front = new BrowsingContextTargetFront(this._client);
    front.actorID = form.actor;
    front.form(form);
    this.manage(front);
    return front;
  }

  async getTarget() {
    if (this._frameTargetFront && this._frameTargetFront.actorID) {
      return this._frameTargetFront;
    }
    if (this._targetFrontPromise) {
      return this._targetFrontPromise;
    }
    this._targetFrontPromise = (async () => {
      try {
        const targetForm = await super.getTarget();
        if (targetForm.error) {
          this._targetFrontPromise = null;
          return null;
        }
        this._frameTargetFront = await this._createFrameTarget(targetForm);
        await this._frameTargetFront.attach();
        // clear the promise if we are finished so that we can re-connect if
        // necessary
        this._targetFrontPromise = null;
        return this._frameTargetFront;
      } catch (e) {
        // This is likely to happen if we get a lot of events which drop previous
        // frames.
        console.log(
          `Request to connect to frameDescriptor "${this.id}" failed: ${e}`
        );
        return null;
      }
    })();
    return this._targetFrontPromise;
  }

  destroy() {
    this._frameTargetFront = null;
    this._targetFrontPromise = null;
    super.destroy();
  }
}

exports.FrameDescriptorFront = FrameDescriptorFront;
registerFront(FrameDescriptorFront);
