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
loader.lazyRequireGetter(
  this,
  "BrowsingContextTargetFront",
  "devtools/client/fronts/targets/browsing-context",
  true
);

class WebExtensionDescriptorFront extends FrontClassWithSpec(
  webExtensionDescriptorSpec
) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);
    this.client = client;
    this.traits = {};
  }

  form(json) {
    this.actorID = json.actor;

    // Do not use `form` name to avoid colliding with protocol.js's `form` method
    this._form = json;
    this.traits = json.traits || {};

    // We used to manipulate the form rather than the front itself.
    // Expose all form attributes to ease accessing them.
    for (const name in json) {
      if (name == "actor") {
        continue;
      }
      this[name] = json[name];
    }
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
        "Tried to call getTarget() for an addon which is not a webextension: " +
          this.actorID
      );
    }

    const form = await super.getTarget();
    const front = new BrowsingContextTargetFront(this.conn, null, this);
    front.form(form);
    this.manage(front);
    return front;
  }
}

exports.WebExtensionDescriptorFront = WebExtensionDescriptorFront;
registerFront(WebExtensionDescriptorFront);
