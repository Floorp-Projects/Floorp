/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  Front,
  FrontClassWithSpec,
} = require("devtools/shared/protocol");
const { promisesSpec } = require("devtools/shared/specs/promises");

/**
 * PromisesFront, the front for the PromisesActor.
 */
const PromisesFront = FrontClassWithSpec(promisesSpec, {
  initialize: function (client, form) {
    Front.prototype.initialize.call(this, client, form);
    this.actorID = form.promisesActor;
    this.manage(this);
  },

  destroy: function () {
    Front.prototype.destroy.call(this);
  }
});

exports.PromisesFront = PromisesFront;
