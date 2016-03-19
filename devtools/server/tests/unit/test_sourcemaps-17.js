/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check that we properly handle frames that cannot be sourcemapped
 * when using sourcemaps.
 */

var gDebuggee;
var gClient;
var gThreadClient;

const {SourceNode} = require("source-map");

function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-source-map");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient, "test-source-map", function(aResponse, aTabClient, aThreadClient) {
      gThreadClient = aThreadClient;
      test_source_map();
    });
  });
  do_test_pending();
}

function test_source_map() {
   // Set up debuggee code.
  const a = new SourceNode(1, 1, "foo.js", "function a() { b(); }");
  const b = new SourceNode(null, null, null, "function b() { c(); }");
  const c = new SourceNode(2, 1, "foo.js", "function c() { debugger; }");
  const { map, code } = (new SourceNode(null, null, null, [a,b,c])).toStringWithSourceMap({
    file: "bar.js",
    sourceRoot: "root",
  });
  Components.utils.evalInSandbox(
    code + "//# sourceMappingURL=data:text/json;base64," + btoa(map.toString()),
    gDebuggee,
    "1.8",
    "http://example.com/www/js/abc.js",
    1
  );

  gThreadClient.addOneTimeListener("paused", function (aEvent, aPacket) {
    gThreadClient.fillFrames(50, function(res) {
      do_check_true(!res.error, "Should not get an error: " + res.error);
      finishClient(gClient);
    })
  });

    // Trigger it.
  gDebuggee.eval("a()");
}
