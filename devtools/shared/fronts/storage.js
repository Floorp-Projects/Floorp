/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { FrontClassWithSpec, registerFront } = require("devtools/shared/protocol");
const { childSpecs, storageSpec } = require("devtools/shared/specs/storage");

for (const childSpec of Object.values(childSpecs)) {
  class ChildStorageFront extends FrontClassWithSpec(childSpec) {
    form(form, detail) {
      if (detail === "actorid") {
        this.actorID = form;
        return null;
      }

      this.actorID = form.actor;
      this.hosts = form.hosts;
      return null;
    }
  }
  registerFront(ChildStorageFront);
}

class StorageFront extends FrontClassWithSpec(storageSpec) {
  constructor(client, tabForm) {
    super(client, { actor: tabForm.storageActor });
    this.manage(this);
  }
}

exports.StorageFront = StorageFront;
registerFront(StorageFront);
