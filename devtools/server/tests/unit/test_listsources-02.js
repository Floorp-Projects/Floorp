/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check getting sources before there are any.
 */

var gClient;
var gThreadClient;

var gNumTimesSourcesSent = 0;

function run_test() {
  initTestDebuggerServer();
  addTestGlobal("test-stack");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.request = (function (origRequest) {
    return function (request, onResponse) {
      if (request.type === "sources") {
        ++gNumTimesSourcesSent;
      }
      return origRequest.call(this, request, onResponse);
    };
  }(gClient.request));
  gClient.connect().then(function () {
    attachTestTabAndResume(gClient, "test-stack",
                           function (response, tabClient, threadClient) {
                             gThreadClient = threadClient;
                             test_listing_zero_sources();
                           });
  });
  do_test_pending();
}

function test_listing_zero_sources() {
  gThreadClient.getSources(function (packet) {
    Assert.ok(!packet.error);
    Assert.ok(!!packet.sources);
    Assert.equal(packet.sources.length, 0);

    Assert.ok(gNumTimesSourcesSent <= 1,
              "Should only send one sources request at most, even though we"
              + " might have had to send one to determine feature support.");

    finishClient(gClient);
  });
}
