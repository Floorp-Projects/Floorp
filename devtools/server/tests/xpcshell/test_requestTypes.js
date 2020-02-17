/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { RootActor } = require("devtools/server/actors/root");

function test_requestTypes_request(client, anActor) {
  client.mainRoot.requestTypes().then(function(response) {
    const expectedRequestTypes = Object.keys(RootActor.prototype.requestTypes);

    Assert.ok(Array.isArray(response.requestTypes));
    Assert.equal(
      JSON.stringify(response.requestTypes),
      JSON.stringify(expectedRequestTypes)
    );

    client.close().then(() => {
      do_test_finished();
    });
  });
}

function run_test() {
  DevToolsServer.init();
  DevToolsServer.registerAllActors();

  const client = new DevToolsClient(DevToolsServer.connectPipe());
  client.connect().then(function() {
    test_requestTypes_request(client);
  });

  do_test_pending();
}
