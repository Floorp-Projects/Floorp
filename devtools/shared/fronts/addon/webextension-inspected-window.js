/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  webExtensionInspectedWindowSpec,
} = require("devtools/shared/specs/addon/webextension-inspected-window");

const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");

/**
 * The corresponding Front object for the WebExtensionInspectedWindowActor.
 */
class WebExtensionInspectedWindowFront extends FrontClassWithSpec(
  webExtensionInspectedWindowSpec
) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);

    // Attribute name from which to retrieve the actorID out of the target actor's form
    this.formAttributeName = "webExtensionInspectedWindowActor";
  }
}

exports.WebExtensionInspectedWindowFront = WebExtensionInspectedWindowFront;
registerFront(WebExtensionInspectedWindowFront);
