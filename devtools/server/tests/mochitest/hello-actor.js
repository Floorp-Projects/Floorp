/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const protocol = require("devtools/shared/protocol");

var HelloActor = protocol.ActorClass({
  typeName: "helloActor",

  initialize: function() {
    protocol.Actor.prototype.initialize.apply(this, arguments);
    this.counter = 0;
  },

  count: protocol.method(function () {
    return ++this.counter;
  }, {
    request: {},
    response: {count: protocol.RetVal("number")}
  })
});
