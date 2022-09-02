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
const DESCRIPTOR_TYPES = require("devtools/client/fronts/descriptors/descriptor-types");
loader.lazyRequireGetter(
  this,
  "WindowGlobalTargetFront",
  "devtools/client/fronts/targets/window-global",
  true
);

class WebExtensionDescriptorFront extends DescriptorMixin(
  FrontClassWithSpec(webExtensionDescriptorSpec)
) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);
    this.traits = {};
  }

  descriptorType = DESCRIPTOR_TYPES.EXTENSION;

  form(json) {
    this.actorID = json.actor;

    // Do not use `form` name to avoid colliding with protocol.js's `form` method
    this._form = json;
    this.traits = json.traits || {};
  }

  get backgroundScriptStatus() {
    return this._form.backgroundScriptStatus;
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

  get persistentBackgroundScript() {
    return this._form.persistentBackgroundScript;
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

  isServerTargetSwitchingEnabled() {
    // For now, we don't expose any target out of the WatcherActor.
    // And the top level target is still manually instantiated by the descriptor.
    // We most likely need to wait for full enabling of EFT before being able to spawn
    // the extension target from the server side as doing this would most likely break
    // the iframe dropdown. It would break it as spawning the targets from the server
    // would probably mean getting rid of the usage of WindowGlobalTargetActor._setWindow
    // and instead spawn one target per extension document.
    // That, instead of having a unique target for all the documents.
    return false;
  }

  getWatcher() {
    return super.getWatcher({
      isServerTargetSwitchingEnabled: this.isServerTargetSwitchingEnabled(),
    });
  }

  _createWebExtensionTarget(form) {
    const front = new WindowGlobalTargetFront(this.conn, null, this);
    front.form(form);
    this.manage(front);
    return front;
  }

  /**
   * Retrieve the WindowGlobalTargetFront representing a
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
