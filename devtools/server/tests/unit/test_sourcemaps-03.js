/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check setting breakpoints in source mapped sources.
 */

var gDebuggee;
var gClient;
var gThreadClient;

const {SourceNode} = require("source-map");

function run_test()
{
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-source-map");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient, "test-source-map", function(aResponse, aTabClient, aThreadClient) {
      gThreadClient = aThreadClient;
      test_simple_source_map();
    });
  });
  do_test_pending();
}

function testBreakpointMapping(aName, aCallback)
{
  Task.spawn(function*() {
    let response = yield waitForPause(gThreadClient);
    do_check_eq(response.why.type, "debuggerStatement");

    const source = yield getSource(gThreadClient, "http://example.com/www/js/" + aName + ".js");
    response = yield setBreakpoint(source, {
      // Setting the breakpoint on an empty line so that it is pushed down one
      // line and we can check the source mapped actualLocation later.
      line: 3
    });

    // Should not slide breakpoints for sourcemapped sources
    do_check_true(!response.actualLocation);

    yield setBreakpoint(source, { line: 4 });

    // The eval will cause us to resume, then we get an unsolicited pause
    // because of our breakpoint, we resume again to finish the eval, and
    // finally receive our last pause which has the result of the client
    // evaluation.
    response = yield gThreadClient.eval(null, aName + "()");
    do_check_eq(response.type, "resumed");

    response = yield waitForPause(gThreadClient);
    do_check_eq(response.why.type, "breakpoint");
    // Assert that we paused because of the breakpoint at the correct
    // location in the code by testing that the value of `ret` is still
    // undefined.
    do_check_eq(response.frame.environment.bindings.variables.ret.value.type,
                "undefined");

    response = yield resume(gThreadClient);

    response = yield waitForPause(gThreadClient);
    do_check_eq(response.why.type, "clientEvaluated");
    do_check_eq(response.why.frameFinished.return, aName);

    response = yield resume(gThreadClient);

    aCallback();
  });

  gDebuggee.eval("(" + function () {
    debugger;
  } + "());");
}

function test_simple_source_map()
{
  let expectedSources = new Set([
    "http://example.com/www/js/a.js",
    "http://example.com/www/js/b.js",
    "http://example.com/www/js/c.js"
  ]);

  gThreadClient.addListener("newSource", function _onNewSource(aEvent, aPacket) {
    expectedSources.delete(aPacket.source.url);
    if (expectedSources.size > 0) {
      return;
    }
    gThreadClient.removeListener("newSource", _onNewSource);

    testBreakpointMapping("a", function () {
      testBreakpointMapping("b", function () {
        testBreakpointMapping("c", function () {
          finishClient(gClient);
        });
      });
    });
  });

  let a = new SourceNode(null, null, null, [
    new SourceNode(1, 0, "a.js", "function a() {\n"),
    new SourceNode(2, 0, "a.js", "  var ret;\n"),
    new SourceNode(3, 0, "a.js", "  // Empty line\n"),
    new SourceNode(4, 0, "a.js", "  ret = 'a';\n"),
    new SourceNode(5, 0, "a.js", "  return ret;\n"),
    new SourceNode(6, 0, "a.js", "}\n")
  ]);
  let b = new SourceNode(null, null, null, [
    new SourceNode(1, 0, "b.js", "function b() {\n"),
    new SourceNode(2, 0, "b.js", "  var ret;\n"),
    new SourceNode(3, 0, "b.js", "  // Empty line\n"),
    new SourceNode(4, 0, "b.js", "  ret = 'b';\n"),
    new SourceNode(5, 0, "b.js", "  return ret;\n"),
    new SourceNode(6, 0, "b.js", "}\n")
  ]);
  let c = new SourceNode(null, null, null, [
    new SourceNode(1, 0, "c.js", "function c() {\n"),
    new SourceNode(2, 0, "c.js", "  var ret;\n"),
    new SourceNode(3, 0, "c.js", "  // Empty line\n"),
    new SourceNode(4, 0, "c.js", "  ret = 'c';\n"),
    new SourceNode(5, 0, "c.js", "  return ret;\n"),
    new SourceNode(6, 0, "c.js", "}\n")
  ]);

  let { code, map } = (new SourceNode(null, null, null, [
    a, b, c
  ])).toStringWithSourceMap({
    file: "http://example.com/www/js/abc.js",
    sourceRoot: "http://example.com/www/js/"
  });

  code += "//# sourceMappingURL=data:text/json;base64," + btoa(map.toString());

  Components.utils.evalInSandbox(code, gDebuggee, "1.8",
                                 "http://example.com/www/js/abc.js", 1);
}
