/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* exported HelloActor */
"use strict";

const protocol = require("devtools/shared/protocol");

const helloSpec = protocol.generateActorSpec({
  typeName: "helloActor",

  methods: {
    count: {
      request: {},
      response: { count: protocol.RetVal("number") },
    },
  },
});

var HelloActor = protocol.ActorClassWithSpec(helloSpec, {
  initialize() {
    protocol.Actor.prototype.initialize.apply(this, arguments);
    this.counter = 0;
  },

  count() {
    return ++this.counter;
  },
});
