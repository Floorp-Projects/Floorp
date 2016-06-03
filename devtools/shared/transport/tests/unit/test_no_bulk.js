/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function run_test() {
  DebuggerServer.registerModule("xpcshell-test/testactors-no-bulk");
  // Allow incoming connections.
  DebuggerServer.init();

  add_task(function* () {
    yield test_bulk_send_error(socket_transport);
    yield test_bulk_send_error(local_transport);
    DebuggerServer.destroy();
  });

  run_next_test();
}

/** * Tests ***/

var test_bulk_send_error = Task.async(function* (transportFactory) {
  let deferred = promise.defer();
  let transport = yield transportFactory();

  let client = new DebuggerClient(transport);
  return client.connect().then(([app, traits]) => {
    do_check_false(traits.bulk);

    try {
      client.startBulkRequest();
      do_throw(new Error("Can't use bulk since server doesn't support it"));
    } catch (e) {
      do_check_true(true);
    }

  });
});
