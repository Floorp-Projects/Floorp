/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Front, FrontClassWithSpec } = require("devtools/server/protocol.js");
const { originalSourceSpec } = require("devtools/shared/specs/stylesheets.js");

/**
 * The client-side counterpart for an OriginalSourceActor.
 */
const OriginalSourceFront = FrontClassWithSpec(originalSourceSpec, {
  initialize: function (client, form) {
    Front.prototype.initialize.call(this, client, form);

    this.isOriginalSource = true;
  },

  form: function (form, detail) {
    if (detail === "actorid") {
      this.actorID = form;
      return;
    }
    this.actorID = form.actor;
    this._form = form;
  },

  get href() {
    return this._form.url;
  },
  get url() {
    return this._form.url;
  }
});

exports.OriginalSourceFront = OriginalSourceFront;
