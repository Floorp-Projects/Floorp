/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that we source map frame locations for the frame we are paused at.
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
    attachTestTabAndResume(
      gClient, "test-source-map",
      function (response, tabClient, threadClient) {
        gThreadClient = threadClient;
        promise.resolve(define_code())
          .then(run_code)
          .then(test_frame_location)
          .catch(error => {
            dump(error + "\n");
            dump(error.stack);
            Assert.ok(false);
          })
          .then(() => {
            finishClient(gClient);
          });
      });
  });
  do_test_pending();
}

function define_code() {
  let { code, map } = (new SourceNode(null, null, null, [
    new SourceNode(1, 0, "a.js", "function a() {\n"),
    new SourceNode(2, 0, "a.js", "  b();\n"),
    new SourceNode(3, 0, "a.js", "}\n"),
    new SourceNode(1, 0, "b.js", "function b() {\n"),
    new SourceNode(2, 0, "b.js", "  c();\n"),
    new SourceNode(3, 0, "b.js", "}\n"),
    new SourceNode(1, 0, "c.js", "function c() {\n"),
    new SourceNode(2, 0, "c.js", "  debugger;\n"),
    new SourceNode(3, 0, "c.js", "}\n"),
  ])).toStringWithSourceMap({
    file: "abc.js",
    sourceRoot: "http://example.com/www/js/"
  });

  code += "//# sourceMappingURL=data:text/json," + map.toString();

  Components.utils.evalInSandbox(code, gDebuggee, "1.8",
                                 "http://example.com/www/js/abc.js", 1);
}

function run_code() {
  const d = defer();
  gClient.addOneTimeListener("paused", function (event, packet) {
    d.resolve(packet);
    gThreadClient.resume();
  });
  gDebuggee.a();
  return d.promise;
}

function test_frame_location({ frame: { where: { source, line, column } } }) {
  Assert.equal(source.url, "http://example.com/www/js/c.js");
  Assert.equal(line, 2);
  Assert.equal(column, 0);
}
