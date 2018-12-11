/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {addonsSpec} = require("devtools/shared/specs/addon/addons");
const { FrontClassWithSpec, registerFront } = require("devtools/shared/protocol");

class AddonsFront extends FrontClassWithSpec(addonsSpec) {
  constructor(client, {addonsActor}) {
    super(client);
    this.actorID = addonsActor;
    this.manage(this);
  }
}

exports.AddonsFront = AddonsFront;
registerFront(AddonsFront);
