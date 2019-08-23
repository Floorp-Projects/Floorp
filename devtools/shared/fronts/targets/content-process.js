/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  contentProcessTargetSpec,
} = require("devtools/shared/specs/targets/content-process");
const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");
const { TargetMixin } = require("./target-mixin");

class ContentProcessTargetFront extends TargetMixin(
  FrontClassWithSpec(contentProcessTargetSpec)
) {
  constructor(client) {
    super(client);

    this.traits = {};
  }

  form(json) {
    this.actorID = json.actor;

    // Save the full form for Target class usage.
    // Do not use `form` name to avoid colliding with protocol.js's `form` method
    this.targetForm = json;
    // FF69 chromeDebugger naming has been renamed into threadActor and could be removed when FF69
    // is no longer supported
    this._threadActor = json.threadActor || json.chromeDebugger;
    this.targetForm.contextActor = this._threadActor;
  }

  get name() {
    return `Content Process ${this.processID}`;
  }

  attach() {
    // All target actors have a console actor to attach.
    // All but xpcshell test actors... which is using a ContentProcessTargetActor
    if (this.targetForm.consoleActor) {
      return this.attachConsole();
    }
    return Promise.resolve();
  }

  reconfigure() {
    // Toolbox and options panel are calling this method but Worker Target can't be
    // reconfigured. So we ignore this call here.
    return Promise.resolve();
  }
}

exports.ContentProcessTargetFront = ContentProcessTargetFront;
registerFront(exports.ContentProcessTargetFront);
