/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check basic source map integration with the "sources" packet in the RDP.
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
                             test_simple_source_map();
                           });
  });
  do_test_pending();
}

function test_simple_source_map() {
  // Because we are source mapping, we should be notified of a.js, b.js, and
  // c.js as sources, and shouldn"t receive abc.js or test_sourcemaps-01.js.
  let expectedSources = new Set(["http://example.com/www/js/a.js",
                                 "http://example.com/www/js/b.js",
                                 "http://example.com/www/js/c.js"]);

  gClient.addOneTimeListener("paused", function (event, packet) {
    gThreadClient.getSources(function (response) {
      Assert.ok(!response.error, "Should not get an error");

      for (let s of response.sources) {
        Assert.notEqual(s.url, "http://example.com/www/js/abc.js",
                        "Shouldn't get the generated source's url.");
        expectedSources.delete(s.url);
      }

      Assert.equal(expectedSources.size, 0,
                   "Should have found all the expected sources sources by now.");

      finishClient(gClient);
    });
  });

  let { code, map } = (new SourceNode(null, null, null, [
    new SourceNode(1, 0, "a.js", "function a() { return 'a'; }\n"),
    new SourceNode(1, 0, "b.js", "function b() { return 'b'; }\n"),
    new SourceNode(1, 0, "c.js", "function c() { return 'c'; }\n"),
    new SourceNode(1, 0, "d.js", "debugger;\n")
  ])).toStringWithSourceMap({
    file: "abc.js",
    sourceRoot: "http://example.com/www/js/"
  });

  code += "//# sourceMappingURL=data:text/json;base64," + btoa(map.toString());

  Components.utils.evalInSandbox(code, gDebuggee, "1.8",
                                 "http://example.com/www/js/abc.js", 1);
}
