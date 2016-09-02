/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { RootActor } = require("devtools/server/actors/root");

function test_requestTypes_request(aClient, anActor)
{
  aClient.request({ to: "root", type: "requestTypes" }, function (aResponse) {
    var expectedRequestTypes = Object.keys(RootActor.
                                           prototype.
                                           requestTypes);

    do_check_true(Array.isArray(aResponse.requestTypes));
    do_check_eq(JSON.stringify(aResponse.requestTypes),
                JSON.stringify(expectedRequestTypes));

    aClient.close().then(() => {
      do_test_finished();
    });
  });
}

function run_test()
{
  DebuggerServer.init();
  DebuggerServer.addBrowserActors();

  var client = new DebuggerClient(DebuggerServer.connectPipe());
  client.connect().then(function () {
    test_requestTypes_request(client);
  });

  do_test_pending();
}
