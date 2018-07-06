/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function run_test() {
  const { createRootActor } = require("xpcshell-test/testactors-no-bulk");
  DebuggerServer.setRootActor(createRootActor);
  // Allow incoming connections.
  DebuggerServer.init();

  add_task(async function() {
    await test_bulk_send_error(socket_transport);
    await test_bulk_send_error(local_transport);
    DebuggerServer.destroy();
  });

  run_next_test();
}

/** * Tests ***/

var test_bulk_send_error = async function(transportFactory) {
  const transport = await transportFactory();

  const client = new DebuggerClient(transport);
  return client.connect().then(([app, traits]) => {
    Assert.ok(!traits.bulk);

    try {
      client.startBulkRequest();
      do_throw(new Error("Can't use bulk since server doesn't support it"));
    } catch (e) {
      Assert.ok(true);
    }
  });
};
