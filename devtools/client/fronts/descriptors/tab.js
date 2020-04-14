/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { tabDescriptorSpec } = require("devtools/shared/specs/descriptors/tab");

loader.lazyRequireGetter(
  this,
  "BrowsingContextTargetFront",
  "devtools/client/fronts/targets/browsing-context",
  true
);
loader.lazyRequireGetter(
  this,
  "LocalTabTargetFront",
  "devtools/client/fronts/targets/local-tab",
  true
);
const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");

class TabDescriptorFront extends FrontClassWithSpec(tabDescriptorSpec) {
  constructor(client, targetFront, parentFront, options) {
    super(client, targetFront, parentFront);
    this._client = client;
  }

  form(json) {
    this.actorID = json.actor;
    this.selected = json.selected;
    this.traits = json.traits || {};
  }

  _createTabTarget(form, filter) {
    // Instanciate a specialized class for a local tab as it needs some more
    // client side integration with the Firefox frontend.
    // But ignore the fake `tab` object we receive, where there is only a
    // `linkedBrowser` attribute, but this isn't a real <tab> element.
    // devtools/client/framework/test/browser_toolbox_target.js is passing such
    // a fake tab.
    let front;
    if (filter?.tab?.tagName == "tab") {
      front = new LocalTabTargetFront(this._client, null, this, filter.tab);
    } else {
      front = new BrowsingContextTargetFront(this._client, null, this);
    }
    // As these fronts aren't instantiated by protocol.js, we have to set their actor ID
    // manually like that:
    front.actorID = form.actor;
    front.form(form);
    this.manage(front);
    return front;
  }

  async getTarget(filter) {
    const form = await super.getTarget();
    let front = this.actor(form.actor);
    if (front) {
      front.form(form);
      return front;
    }
    front = this._createTabTarget(form, filter);
    await front.attach();
    return front;
  }
}

exports.TabDescriptorFront = TabDescriptorFront;
registerFront(TabDescriptorFront);
