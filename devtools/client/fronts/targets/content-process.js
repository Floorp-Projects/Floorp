/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  contentProcessTargetSpec,
} = require("resource://devtools/shared/specs/targets/content-process.js");
const {
  FrontClassWithSpec,
  registerFront,
} = require("resource://devtools/shared/protocol.js");
const {
  TargetMixin,
} = require("resource://devtools/client/fronts/targets/target-mixin.js");

class ContentProcessTargetFront extends TargetMixin(
  FrontClassWithSpec(contentProcessTargetSpec)
) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);

    this.traits = {};
  }

  form(json) {
    this.actorID = json.actor;
    this.processID = json.processID;

    // Save the full form for Target class usage.
    // Do not use `form` name to avoid colliding with protocol.js's `form` method
    this.targetForm = json;

    this.remoteType = json.remoteType;
    this.isXpcShellTarget = json.isXpcShellTarget;
  }

  get name() {
    // @backward-compat { version 87 } We now have `remoteType` attribute.
    if (this.remoteType) {
      return `(pid ${this.processID}) ${this.remoteType.replace(
        "webIsolated=",
        ""
      )}`;
    }
    return `(pid ${this.processID}) Content Process`;
  }

  reconfigure() {
    // Toolbox and options panel are calling this method but Worker Target can't be
    // reconfigured. So we ignore this call here.
    return Promise.resolve();
  }
}

exports.ContentProcessTargetFront = ContentProcessTargetFront;
registerFront(exports.ContentProcessTargetFront);
