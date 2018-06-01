/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const protocol = require("devtools/shared/protocol");
const specs = require("devtools/shared/specs/storage");

for (const childSpec of Object.values(specs.childSpecs)) {
  protocol.FrontClassWithSpec(childSpec, {
    form(form, detail) {
      if (detail === "actorid") {
        this.actorID = form;
        return null;
      }

      this.actorID = form.actor;
      this.hosts = form.hosts;
      return null;
    }
  });
}

const StorageFront = protocol.FrontClassWithSpec(specs.storageSpec, {
  initialize(client, tabForm) {
    protocol.Front.prototype.initialize.call(this, client);
    this.actorID = tabForm.storageActor;
    this.manage(this);
  }
});

exports.StorageFront = StorageFront;
