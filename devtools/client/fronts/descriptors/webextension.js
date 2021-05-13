/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  webExtensionDescriptorSpec,
} = require("devtools/shared/specs/descriptors/webextension");
const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");
const {
  DescriptorMixin,
} = require("devtools/client/fronts/descriptors/descriptor-mixin");
loader.lazyRequireGetter(
  this,
  "BrowsingContextTargetFront",
  "devtools/client/fronts/targets/browsing-context",
  true
);

class WebExtensionDescriptorFront extends DescriptorMixin(
  FrontClassWithSpec(webExtensionDescriptorSpec)
) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);
    this.traits = {};
  }

  form(json) {
    this.actorID = json.actor;

    // Do not use `form` name to avoid colliding with protocol.js's `form` method
    this._form = json;
    this.traits = json.traits || {};
  }

  get debuggable() {
    return this._form.debuggable;
  }

  get hidden() {
    return this._form.hidden;
  }

  get iconDataURL() {
    return this._form.iconDataURL;
  }

  get iconURL() {
    return this._form.iconURL;
  }

  get id() {
    return this._form.id;
  }

  get isSystem() {
    return this._form.isSystem;
  }

  get isWebExtensionDescriptor() {
    return true;
  }

  get isWebExtension() {
    return this._form.isWebExtension;
  }

  get manifestURL() {
    return this._form.manifestURL;
  }

  get name() {
    return this._form.name;
  }

  get temporarilyInstalled() {
    return this._form.temporarilyInstalled;
  }

  get url() {
    return this._form.url;
  }

  get warnings() {
    return this._form.warnings;
  }

  _createWebExtensionTarget(form) {
    const front = new BrowsingContextTargetFront(this.conn, null, this);
    front.form(form);
    this.manage(front);
    return front;
  }

  /**
   * Retrieve the BrowsingContextTargetFront representing a
   * WebExtensionTargetActor if this addon is a webextension.
   *
   * WebExtensionDescriptors will be created for any type of addon type
   * (webextension, search plugin, themes). Only webextensions can be targets.
   * This method will throw for other addon types.
   *
   * TODO: We should filter out non-webextension & non-debuggable addons on the
   * server to avoid the isWebExtension check here. See Bug 1644355.
   */
  async getTarget() {
    if (!this.isWebExtension) {
      throw new Error(
        "Tried to create a target for an addon which is not a webextension: " +
          this.actorID
      );
    }

    if (this._targetFront && !this._targetFront.isDestroyed()) {
      return this._targetFront;
    }

    if (this._targetFrontPromise) {
      return this._targetFrontPromise;
    }

    this._targetFrontPromise = (async () => {
      let targetFront = null;
      try {
        const targetForm = await super.getTarget();
        targetFront = this._createWebExtensionTarget(targetForm);
        await targetFront.attach();
      } catch (e) {
        console.log(
          `Request to connect to WebExtensionDescriptor "${this.id}" failed: ${e}`
        );
      }
      this._targetFront = targetFront;
      this._targetFrontPromise = null;
      return targetFront;
    })();
    return this._targetFrontPromise;
  }
}

exports.WebExtensionDescriptorFront = WebExtensionDescriptorFront;
registerFront(WebExtensionDescriptorFront);
