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
  gClient.connect().then(function () {
    attachTestTabAndResume(gClient, "test-source-map", function (aResponse, aTabClient, aThreadClient) {
      gThreadClient = aThreadClient;
      test_source_map();
    });
  });
  do_test_pending();
}

function test_source_map() {
   // Set up debuggee code.
  const a = new SourceNode(1, 1, "a.js", "function a() { b(); }");
  const b = new SourceNode(null, null, null, "function b() { c(); }");
  const c = new SourceNode(1, 1, "c.js", "function c() { d(); }");
  const d = new SourceNode(null, null, null, "function d() { e(); }");
  const e = new SourceNode(1, 1, "e.js", "function e() { debugger; }");
  const { map, code } = (new SourceNode(null, null, null, [a, b, c, d, e])).toStringWithSourceMap({
    file: "root.js",
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
    gThreadClient.getFrames(0, 50, function ({ error, frames }) {
      do_check_true(!error);
      do_check_eq(frames.length, 4);
      // b.js should be skipped
      do_check_eq(frames[0].where.source.url, "http://example.com/www/root/e.js");
      do_check_eq(frames[1].where.source.url, "http://example.com/www/root/c.js");
      do_check_eq(frames[2].where.source.url, "http://example.com/www/root/a.js");
      do_check_eq(frames[3].where.source.url, null);

      finishClient(gClient);
    });
  });

    // Trigger it.
  gDebuggee.eval("a()");
}
