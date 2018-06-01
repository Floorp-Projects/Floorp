/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var gClient;
var gDebuggee;

function run_test() {
  initTestDebuggerServer();
  gDebuggee = testGlobal("test-1");
  DebuggerServer.addTestGlobal(gDebuggee);

  const transport = DebuggerServer.connectPipe();
  gClient = new DebuggerClient(transport);
  gClient.connect().then(function(type, traits) {
    attachTestTab(gClient, "test-1", function(reply, tabClient) {
      test_close(transport);
    });
  });
  do_test_pending();
}

function test_close(transport) {
  // Check that, if we fake a transport shutdown
  // (like if a device is unplugged)
  // the client is automatically closed,
  // and we can still call client.close.
  const onClosed = function() {
    gClient.removeListener("closed", onClosed);
    ok(true, "Client emitted 'closed' event");
    gClient.close().then(function() {
      ok(true, "client.close() successfully called its callback");
      do_test_finished();
    });
  };
  gClient.addListener("closed", onClosed);
  transport.close();
}
