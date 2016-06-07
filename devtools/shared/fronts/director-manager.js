/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  messagePortSpec,
  directorScriptSpec,
  directorManagerSpec,
} = require("devtools/shared/specs/director-manager");
const protocol = require("devtools/shared/protocol");

/**
 * The corresponding Front object for the MessagePortActor.
 */
const MessagePortFront = protocol.FrontClassWithSpec(messagePortSpec, {
  initialize: function (client, form) {
    protocol.Front.prototype.initialize.call(this, client, form);
  }
});

exports.MessagePortFront = MessagePortFront;

/**
 * The corresponding Front object for the DirectorScriptActor.
 */
const DirectorScriptFront = protocol.FrontClassWithSpec(directorScriptSpec, {
  initialize: function (client, form) {
    protocol.Front.prototype.initialize.call(this, client, form);
  }
});

exports.DirectorScriptFront = DirectorScriptFront;

/**
 * The corresponding Front object for the DirectorManagerActor.
 */
const DirectorManagerFront = protocol.FrontClassWithSpec(directorManagerSpec, {
  initialize: function (client, { directorManagerActor }) {
    protocol.Front.prototype.initialize.call(this, client, {
      actor: directorManagerActor
    });
    this.manage(this);
  }
});

exports.DirectorManagerFront = DirectorManagerFront;
