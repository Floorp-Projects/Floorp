/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  processDescriptorSpec,
} = require("devtools/shared/specs/descriptors/process");
const {
  BrowsingContextTargetFront,
} = require("devtools/shared/fronts/targets/browsing-context");
const {
  ContentProcessTargetFront,
} = require("devtools/shared/fronts/targets/content-process");
const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");

class ProcessDescriptorFront extends FrontClassWithSpec(processDescriptorSpec) {
  constructor(client) {
    super(client);
    this.isParent = false;
    this._processTargetFront = null;
    this._targetFrontPromise = null;
    this._client = client;
  }

  form(json) {
    this.id = json.id;
    this.isParent = json.isParent;
  }

  async _createProcessTargetFront(form) {
    let front = null;
    // the request to getTarget may return a ContentProcessTargetActor or a
    // ParentProcessTargetActor. In most cases getProcess(0) will return the
    // main process target actor, which is a ParentProcessTargetActor, but
    // not in xpcshell, which uses a ContentProcessTargetActor. So select
    // the right front based on the actor ID.
    if (form.actor.includes("parentProcessTarget")) {
      // ParentProcessTargetActor doesn't have a specific front, instead it uses
      // BrowsingContextTargetFront on the client side.
      front = new BrowsingContextTargetFront(this._client);
    } else {
      front = new ContentProcessTargetFront(this._client);
    }
    // As these fronts aren't instantiated by protocol.js, we have to set their actor ID
    // manually like that:
    front.actorID = form.actor;
    front.form(form);
    front.processID = this.id;
    this.manage(front);
    return front;
  }

  async getTarget() {
    if (this._processTargetFront && this._processTargetFront.actorID) {
      return this._processTargetFront;
    }
    if (this._targetFrontPromise) {
      return this._targetFrontPromise;
    }
    this._targetFrontPromise = (async () => {
      try {
        const targetForm = await super.getTarget();
        this._processTargetFront = await this._createProcessTargetFront(
          targetForm
        );
        await this._processTargetFront.attach();
        // clear the promise if we are finished so that we can re-connect if
        // necessary
        this._targetFrontPromise = null;
        return this._processTargetFront;
      } catch (e) {
        // This is likely to happen if we get a lot of events which drop previous
        // processes.
        console.log(
          `Request to connect to ProcessDescriptor "${this.id}" failed: ${e}`
        );
        return null;
      }
    })();
    return this._targetFrontPromise;
  }

  destroy() {
    this._processTargetFront = null;
    this._targetFrontPromise = null;
    super.destroy();
  }
}

exports.ProcessDescriptorFront = ProcessDescriptorFront;
registerFront(ProcessDescriptorFront);
