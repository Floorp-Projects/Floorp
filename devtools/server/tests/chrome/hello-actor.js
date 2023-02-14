/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* exported HelloActor */
"use strict";

const protocol = require("resource://devtools/shared/protocol.js");

const helloSpec = protocol.generateActorSpec({
  typeName: "helloActor",

  methods: {
    count: {
      request: {},
      response: { count: protocol.RetVal("number") },
    },
  },
});

class HelloActor extends protocol.Actor {
  constructor(conn) {
    super(conn, helloSpec);
    this.counter = 0;
  }

  count() {
    return ++this.counter;
  }
}
