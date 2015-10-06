/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const protocol = require("devtools/server/protocol");

var HelloActor = protocol.ActorClass({
  typeName: "helloActor",

  hello: protocol.method(function () {
    return;
  }, {
    request: {},
    response: {}
  })
});
