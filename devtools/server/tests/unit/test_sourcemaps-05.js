/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check that relative source map urls work.
 */

var gDebuggee;
var gClient;
var gThreadClient;

function run_test()
{
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-source-map");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient, "test-source-map", function(aResponse, aTabClient, aThreadClient) {
      gThreadClient = aThreadClient;
      test_relative_source_map();
    });
  });
  do_test_pending();
}

function test_relative_source_map()
{
  gThreadClient.addOneTimeListener("newSource", function _onNewSource(aEvent, aPacket) {
    do_check_eq(aEvent, "newSource");
    do_check_eq(aPacket.type, "newSource");
    do_check_true(!!aPacket.source);

    do_check_true(aPacket.source.url.indexOf("sourcemapped.coffee") !== -1,
                  "The new source should be a coffee file.");
    do_check_eq(aPacket.source.url.indexOf("sourcemapped.js"), -1,
                "The new source should not be a js file.");

    finishClient(gClient);
  });

  code = readFile("sourcemapped.js")
    + "\n//# sourceMappingURL=source-map-data/sourcemapped.map";

  Components.utils.evalInSandbox(code, gDebuggee, "1.8",
                                 getFileUrl("sourcemapped.js"), 1);
}
