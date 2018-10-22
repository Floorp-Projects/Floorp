/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/**
 * Check getSources functionality with sourcemaps.
 */

const {SourceNode} = require("source-map");

function run_test() {
  run_test_with_server(DebuggerServer, function() {
    // Bug 1304144 - This test does not run in a worker because the
    // `rpc` method which talks to the main thread does not work.
    // run_test_with_server(WorkerDebuggerServer, do_test_finished);
    do_test_finished();
  });
  do_test_pending();
}

function run_test_with_server(server, cb) {
  (async function() {
    initTestDebuggerServer(server);
    const debuggee = addTestGlobal("test-sources", server);
    const client = new DebuggerClient(server.connectPipe());
    await client.connect();
    const [,, threadClient] = await attachTestTabAndResume(client, "test-sources");

    await threadClient.reconfigure({ useSourceMaps: true });
    addSources(debuggee);

    threadClient.getSources(async function(res) {
      Assert.equal(res.sources.length, 3, "3 sources exist");

      await threadClient.reconfigure({ useSourceMaps: false });

      threadClient.getSources(function(res) {
        Assert.equal(res.sources.length, 1, "1 source exist");
        client.close().then(cb);
      });
    });
  })();
}

function addSources(debuggee) {
  let { code, map } = (new SourceNode(null, null, null, [
    new SourceNode(1, 0, "a.js", "function a() { return 'a'; }\n"),
    new SourceNode(1, 0, "b.js", "function b() { return 'b'; }\n"),
    new SourceNode(1, 0, "c.js", "function c() { return 'c'; }\n"),
  ])).toStringWithSourceMap({
    file: "abc.js",
    sourceRoot: "http://example.com/www/js/",
  });

  code += "//# sourceMappingURL=data:text/json;base64," + btoa(map.toString());

  Cu.evalInSandbox(code, debuggee, "1.8",
                   "http://example.com/www/js/abc.js", 1);
}
