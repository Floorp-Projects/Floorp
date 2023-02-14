/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var {
  RetVal,
  Actor,
  FrontClassWithSpec,
  generateActorSpec,
} = require("resource://devtools/shared/protocol.js");

const lazySpec = generateActorSpec({
  typeName: "lazy",

  methods: {
    hello: {
      response: { str: RetVal("string") },
    },
  },
});

class LazyActor extends Actor {
  constructor(conn, id) {
    super(conn, lazySpec);

    Services.obs.notifyObservers(null, "actor", "instantiated");
  }

  hello(str) {
    return "world";
  }
}
exports.LazyActor = LazyActor;

Services.obs.notifyObservers(null, "actor", "loaded");

class LazyFront extends FrontClassWithSpec(lazySpec) {
  constructor(client) {
    super(client);
  }
}
exports.LazyFront = LazyFront;
