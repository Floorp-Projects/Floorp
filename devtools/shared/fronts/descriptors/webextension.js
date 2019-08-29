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
  "devtools/shared/fronts/targets/browsing-context",
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

    // Save the full form for Target class usage.
    // Do not use `form` name to avoid colliding with protocol.js's `form` method
    this.targetForm = json;
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

  connect() {
    return this.getTarget();
  }

  /**
   * Returns the actual target front for web extensions.
   *
   * Instead, we want to use a WebExtensionTargetActor, which
   * inherits from BrowsingContextTargetActor. This connect method is used to retrieve
   * the final target actor to use.
   */
  async getTarget() {
    if (
      this.isWebExtension &&
      this.client.mainRoot.traits.webExtensionAddonConnect
    ) {
      // The Webextension form is related to a WebExtensionActor instance,
      // which isn't a target actor on its own, it is an actor living in the parent
      // process with access to the extension metadata, it can control the extension (e.g.
      // reloading it) and listen to the AddonManager events related to the lifecycle of
      // the addon (e.g. when the addon is disabled or uninstalled).
      // To retrieve the target actor instance, we call its "connect" method, (which
      // fetches the target actor targetForm from a WebExtensionTargetActor instance).
      let form = null;
      // FF70+ The method is now called getTarget`
      if (!this.traits.isDescriptor) {
        form = await super.connect();
      } else {
        form = await super.getTarget();
      }
      const front = new BrowsingContextTargetFront(this.conn, {
        actor: form.actor,
      });
      front.form(form);
      this.manage(front);
      return front;
    }
    return this;
  }
}

exports.WebExtensionDescriptorFront = WebExtensionDescriptorFront;
registerFront(WebExtensionDescriptorFront);
