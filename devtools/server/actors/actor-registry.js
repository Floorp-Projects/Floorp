/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const protocol = require("devtools/shared/protocol");
const { method, custom, Arg, Option, RetVal } = protocol;

const { Cu, CC, components } = require("chrome");
const Services = require("Services");
const { DebuggerServer } = require("devtools/server/main");
const { registerActor, unregisterActor } = require("devtools/server/actors/utils/actor-registry-utils");
const { actorActorSpec, actorRegistrySpec } = require("devtools/shared/specs/actor-registry");

/**
 * The ActorActor gives you a handle to an actor you've dynamically
 * registered and allows you to unregister it.
 */
const ActorActor = protocol.ActorClassWithSpec(actorActorSpec, {
  initialize: function (conn, options) {
    protocol.Actor.prototype.initialize.call(this, conn);

    this.options = options;
  },

  unregister: function () {
    unregisterActor(this.options);
  }
});

/*
 * The ActorRegistryActor allows clients to define new actors on the
 * server. This is particularly useful for addons.
 */
const ActorRegistryActor = protocol.ActorClassWithSpec(actorRegistrySpec, {
  initialize: function (conn) {
    protocol.Actor.prototype.initialize.call(this, conn);
  },

  registerActor: function (sourceText, fileName, options) {
    return registerActor(sourceText, fileName, options).then(() => {
      let { constructor, type } = options;

      return ActorActor(this.conn, {
        name: constructor,
        tab: type.tab,
        global: type.global
      });
    });
  }
});

exports.ActorRegistryActor = ActorRegistryActor;
