/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that we continue stepping when a single original source's line
 * corresponds to multiple generated js lines.
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
    attachTestTabAndResume(gClient, "test-source-map",
                           function (response, tabClient, threadClient) {
                             gThreadClient = threadClient;
                             define_code();
                           });
  });
  do_test_pending();
}

function define_code() {
  let { code, map } = (new SourceNode(null, null, null, [
    new SourceNode(1, 0, "a.js", "function runTest() {\n"),
    // A bunch of js lines map to the same original source line.
    new SourceNode(2, 0, "a.js", "  debugger;\n"),
    new SourceNode(2, 0, "a.js", "  var sum = 0;\n"),
    new SourceNode(2, 0, "a.js", "  for (var i = 0; i < 5; i++) {\n"),
    new SourceNode(2, 0, "a.js", "    sum += i;\n"),
    new SourceNode(2, 0, "a.js", "  }\n"),
    // And now we have a new line in the original source that we should stop at.
    new SourceNode(3, 0, "a.js", "  sum;\n"),
    new SourceNode(3, 0, "a.js", "}\n"),
  ])).toStringWithSourceMap({
    file: "abc.js",
    sourceRoot: "http://example.com/"
  });

  code += "//# sourceMappingURL=data:text/json," + map.toString();

  Components.utils.evalInSandbox(code, gDebuggee, "1.8",
                                 "http://example.com/abc.js", 1);

  run_code();
}

function run_code() {
  gClient.addOneTimeListener("paused", function (event, packet) {
    Assert.equal(packet.why.type, "debuggerStatement");
    step_in();
  });
  gDebuggee.runTest();
}

function step_in() {
  gClient.addOneTimeListener("paused", function (event, packet) {
    Assert.equal(packet.why.type, "resumeLimit");
    let { frame: { environment, where: { source, line } } } = packet;
    // Stepping should have moved us to the next source mapped line.
    Assert.equal(source.url, "http://example.com/a.js");
    Assert.equal(line, 3);
    // Which should have skipped over the for loop in the generated js and sum
    // should be calculated.
    Assert.equal(environment.bindings.variables.sum.value, 10);
    finishClient(gClient);
  });
  gThreadClient.stepIn();
}

