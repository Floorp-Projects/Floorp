/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { extend } = require("devtools/shared/extend");
const { ObjectActorProto } = require("devtools/server/actors/object");
const protocol = require("devtools/shared/protocol");
const { ActorClassWithSpec } = protocol;
const { objectSpec } = require("devtools/shared/specs/object");

/**
 * Protocol.js expects only the prototype object, and does not maintain the prototype
 * chain when it constructs the ActorClass. For this reason we are using extend to
 * maintain the properties of ObjectActorProto.
 **/
const proto = extend({}, ObjectActorProto);

Object.assign(proto, {
  /**
   * Creates a pause-scoped actor for the specified object.
   * @see ObjectActor
   */
  initialize(obj, hooks, conn) {
    ObjectActorProto.initialize.call(this, obj, hooks, conn);
    this.hooks.promote = hooks.promote;
    this.hooks.isThreadLifetimePool = hooks.isThreadLifetimePool;
  },

  isPaused() {
    return this.threadActor ? this.threadActor.state === "paused" : true;
  },

  withPaused(method) {
    return function() {
      if (this.isPaused()) {
        return method.apply(this, arguments);
      }

      return {
        error: "wrongState",
        message:
          this.constructor.name +
          " actors can only be accessed while the thread is paused.",
      };
    };
  },
});

const guardWithPaused = [
  "decompile",
  "displayString",
  "ownPropertyNames",
  "parameterNames",
  "property",
  "prototype",
  "prototypeAndProperties",
  "scope",
];

guardWithPaused.forEach(f => {
  proto[f] = proto.withPaused(ObjectActorProto[f]);
});

Object.assign(proto, {
  /**
   * Handle a protocol request to promote a pause-lifetime grip to a
   * thread-lifetime grip.
   *
   * @param request object
   *        The protocol request object.
   */
  threadGrip: proto.withPaused(function(request) {
    this.hooks.promote();
    return {};
  }),

  /**
   * Handle a protocol request to release a thread-lifetime grip.
   *
   * @param request object
   *        The protocol request object.
   */
  destroy: proto.withPaused(function(request) {
    if (this.hooks.isThreadLifetimePool()) {
      return {
        error: "notReleasable",
        message: "Only thread-lifetime actors can be released.",
      };
    }

    return protocol.Actor.prototype.destroy.call(this);
  }),
});

exports.PauseScopedObjectActor = ActorClassWithSpec(objectSpec, proto);
// ActorClassWithSpec(objectSpec, {...ObjectActorProto, ...proto});
