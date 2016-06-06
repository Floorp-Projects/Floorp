/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {preferenceSpec} = require("devtools/shared/specs/preference");
const protocol = require("devtools/shared/protocol");

const PreferenceFront = protocol.FrontClassWithSpec(preferenceSpec, {
  initialize: function (client, form) {
    protocol.Front.prototype.initialize.call(this, client);
    this.actorID = form.preferenceActor;
    this.manage(this);
  },
});

const _knownPreferenceFronts = new WeakMap();

exports.getPreferenceFront = function (client, form) {
  if (!form.preferenceActor) {
    return null;
  }

  if (_knownPreferenceFronts.has(client)) {
    return _knownPreferenceFronts.get(client);
  }

  let front = new PreferenceFront(client, form);
  _knownPreferenceFronts.set(client, front);
  return front;
};
