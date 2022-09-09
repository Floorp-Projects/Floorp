/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var {
  RetVal,
  Actor,
  ActorClassWithSpec,
  FrontClassWithSpec,
  generateActorSpec,
} = require("devtools/shared/protocol");

const lazySpec = generateActorSpec({
  typeName: "lazy",

  methods: {
    hello: {
      response: { str: RetVal("string") },
    },
  },
});

exports.LazyActor = ActorClassWithSpec(lazySpec, {
  initialize(conn, id) {
    Actor.prototype.initialize.call(this, conn);

    Services.obs.notifyObservers(null, "actor", "instantiated");
  },

  hello(str) {
    return "world";
  },
});

Services.obs.notifyObservers(null, "actor", "loaded");

class LazyFront extends FrontClassWithSpec(lazySpec) {
  constructor(client) {
    super(client);
  }
}
exports.LazyFront = LazyFront;
