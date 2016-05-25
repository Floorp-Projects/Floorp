/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {addonsSpec} = require("devtools/shared/specs/addons");
const protocol = require("devtools/shared/protocol");

const AddonsFront = protocol.FrontClassWithSpec(addonsSpec, {
  initialize: function (client, {addonsActor}) {
    protocol.Front.prototype.initialize.call(this, client);
    this.actorID = addonsActor;
    this.manage(this);
  }
});

exports.AddonsFront = AddonsFront;
