/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { addonTargetSpec } = require("devtools/shared/specs/targets/addon");
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

class AddonTargetFront extends FrontClassWithSpec(addonTargetSpec) {
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

    // We used to manipulate the form rather than the front itself.
    // Expose all form attributes to ease accessing them.
    for (const name in json) {
      if (name == "actor") {
        continue;
      }
      this[name] = json[name];
    }
  }

  isLegacyTemporaryExtension() {
    if (!this.type) {
      // If about:debugging is connected to an older then 59 remote Firefox, and type is
      // not available on the addon/webextension actors, return false to avoid showing
      // irrelevant warning messages.
      return false;
    }
    return (
      this.type == "extension" &&
      this.temporarilyInstalled &&
      !this.isWebExtension &&
      !this.isAPIExtension
    );
  }

  /**
   * Returns the actual target front for web extensions.
   *
   * AddonTargetActor is used for WebExtensions, but this isn't the final target actor
   * we want to use for it. AddonTargetActor only expose metadata about the Add-on, like
   * its name, type, ... Instead, we want to use a WebExtensionTargetActor, which
   * inherits from BrowsingContextTargetActor. This connect method is used to retrive
   * the final target actor to use.
   */
  async connect() {
    if (
      this.isWebExtension &&
      this.client.mainRoot.traits.webExtensionAddonConnect
    ) {
      // The AddonTargetFront form is related to a WebExtensionActor instance,
      // which isn't a target actor on its own, it is an actor living in the parent
      // process with access to the addon metadata, it can control the addon (e.g.
      // reloading it) and listen to the AddonManager events related to the lifecycle of
      // the addon (e.g. when the addon is disabled or uninstalled).
      // To retrieve the target actor instance, we call its "connect" method, (which
      // fetches the target actor targetForm from a WebExtensionTargetActor instance).
      const { form } = await super.connect();
      const front = new BrowsingContextTargetFront(this.client, {
        actor: form.actor,
      });
      front.form(form);
      this.manage(front);
      return front;
    }
    return this;
  }

  async attach() {
    if (this._attach) {
      return this._attach;
    }
    this._attach = (async () => {
      const response = await super.attach();

      this._threadActor = response.threadActor;

      return this.attachConsole();
    })();
    return this._attach;
  }

  reconfigure() {
    // Toolbox and options panel are calling this method but Addon Target can't be
    // reconfigured. So we ignore this call here.
    return Promise.resolve();
  }
}

exports.AddonTargetFront = AddonTargetFront;
registerFront(AddonTargetFront);
