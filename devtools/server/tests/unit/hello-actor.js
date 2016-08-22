/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const protocol = require("devtools/shared/protocol");

const helloSpec = protocol.generateActorSpec({
  typeName: "helloActor",

  methods: {
    hello: {}
  }
});

var HelloActor = protocol.ActorClassWithSpec(helloSpec, {
  hello: function () {
    return;
  }
});
