/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that absolute source map urls work.
 */

var gDebuggee;
var gClient;
var gThreadClient;

function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-source-map");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient, "test-source-map",
                           function(response, tabClient, threadClient) {
                             gThreadClient = threadClient;
                             test_absolute_source_map();
                           });
  });
  do_test_pending();
}

function test_absolute_source_map() {
  gThreadClient.addOneTimeListener("newSource", function _onNewSource(event, packet) {
    Assert.equal(event, "newSource");
    Assert.equal(packet.type, "newSource");
    Assert.ok(!!packet.source);

    Assert.ok(packet.source.url.includes("sourcemapped.coffee"),
              "The new source should be a coffee file.");
    Assert.equal(packet.source.url.indexOf("sourcemapped.js"), -1,
                 "The new source should not be a js file.");

    finishClient(gClient);
  });

  const code = readFile("sourcemapped.js")
    + "\n//# sourceMappingURL=" + getFileUrl("source-map-data/sourcemapped.map");

  Cu.evalInSandbox(code, gDebuggee, "1.8",
                   getFileUrl("sourcemapped.js"), 1);
}
