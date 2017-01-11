/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {settingsSpec} = require("devtools/shared/specs/settings");
const protocol = require("devtools/shared/protocol");

const SettingsFront = protocol.FrontClassWithSpec(settingsSpec, {
  initialize: function (client, form) {
    protocol.Front.prototype.initialize.call(this, client);
    this.actorID = form.settingsActor;
    this.manage(this);
  },
});

const _knownSettingsFronts = new WeakMap();

exports.getSettingsFront = function (client, form) {
  if (!form.settingsActor) {
    return null;
  }
  if (_knownSettingsFronts.has(client)) {
    return _knownSettingsFronts.get(client);
  }
  let front = new SettingsFront(client, form);
  _knownSettingsFronts.set(client, front);
  return front;
};
