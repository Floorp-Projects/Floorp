/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-unused-vars: ["error", {"vars": "local"}] */

"use strict";

const protocol = require("devtools/shared/protocol");

const helloSpec = protocol.generateActorSpec({
  typeName: "helloActor",

  methods: {
    hello: {},
  },
});

var HelloActor = protocol.ActorClassWithSpec(helloSpec, {
  hello() {},
});
