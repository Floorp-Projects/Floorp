/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  webExtensionInspectedWindowSpec,
} = require("devtools/shared/specs/addon/webextension-inspected-window");

const protocol = require("devtools/shared/protocol");

/**
 * The corresponding Front object for the WebExtensionInspectedWindowActor.
 */
const WebExtensionInspectedWindowFront = protocol.FrontClassWithSpec(
  webExtensionInspectedWindowSpec,
  {
    initialize: function(client, { webExtensionInspectedWindowActor }) {
      protocol.Front.prototype.initialize.call(this, client, {
        actor: webExtensionInspectedWindowActor
      });
      this.manage(this);
    }
  }
);

exports.WebExtensionInspectedWindowFront = WebExtensionInspectedWindowFront;
