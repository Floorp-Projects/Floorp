/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const protocol = require("devtools/shared/protocol");

const helloSpec = protocol.generateActorSpec({
  typeName: "helloActor",

  methods: {
    count: {
      request: {},
      response: {count: protocol.RetVal("number")}
    }
  }
});

var HelloActor = protocol.ActorClassWithSpec(helloSpec, {
  initialize: function () {
    protocol.Actor.prototype.initialize.apply(this, arguments);
    this.counter = 0;
  },

  count: function () {
    return ++this.counter;
  }
});
