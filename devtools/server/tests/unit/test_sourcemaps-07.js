/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/**
 * Test that we don't permanently cache sources from source maps.
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
                             test_cached_original_sources();
                           });
  });
  do_test_pending();
}

function test_cached_original_sources() {
  writeFile("temp.js", "initial content");

  gThreadClient.addOneTimeListener("newSource", onNewSource);

  let node = new SourceNode(1, 0,
                            getFileUrl("temp.js"),
                            "function funcFromTemp() {}\n");
  let { code, map } = node.toStringWithSourceMap({
    file: "abc.js"
  });
  code += "//# sourceMappingURL=data:text/json;base64," + btoa(map.toString());

  Components.utils.evalInSandbox(code, gDebuggee, "1.8",
                                 "http://example.com/www/js/abc.js", 1);
}

function onNewSource(event, packet) {
  let sourceClient = gThreadClient.source(packet.source);
  sourceClient.source(function (response) {
    Assert.ok(!response.error,
              "Should not be an error grabbing the source");
    Assert.equal(response.source, "initial content",
                 "The correct source content should be sent");

    writeFile("temp.js", "new content");

    sourceClient.source(function (response) {
      Assert.ok(!response.error,
                "Should not be an error grabbing the source");
      Assert.equal(response.source, "new content",
                   "The correct source content should not be cached, " +
                   "so we should get the new content");

      do_get_file("temp.js").remove(false);
      finishClient(gClient);
    });
  });
}
