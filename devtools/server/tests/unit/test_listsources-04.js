/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check getSources functionality with sourcemaps.
 */

const {SourceNode} = require("source-map");

function run_test() {
  run_test_with_server(DebuggerServer, function () {
    // Bug 1304144 - This test does not run in a worker because the
    // `rpc` method which talks to the main thread does not work.
    // run_test_with_server(WorkerDebuggerServer, do_test_finished);
    do_test_finished();
  });
  do_test_pending();
}

function run_test_with_server(server, cb) {
  Task.spawn(function*() {
    initTestDebuggerServer(server);
    const debuggee = addTestGlobal("test-sources", server);
    const client = new DebuggerClient(server.connectPipe());
    yield client.connect();
    const [,,threadClient] = yield attachTestTabAndResume(client, "test-sources");

    yield threadClient.reconfigure({ useSourceMaps: true });
    addSources(debuggee);

    threadClient.getSources(Task.async(function* (res) {
      do_check_true(res.sources.length === 3, "3 sources exist");

      yield threadClient.reconfigure({ useSourceMaps: false });

      threadClient.getSources(function(res) {
        do_check_true(res.sources.length === 1, "1 source exist");
        client.close().then(cb);
      });
    }));
  });
}

function addSources(debuggee) {
  let { code, map } = (new SourceNode(null, null, null, [
    new SourceNode(1, 0, "a.js", "function a() { return 'a'; }\n"),
    new SourceNode(1, 0, "b.js", "function b() { return 'b'; }\n"),
    new SourceNode(1, 0, "c.js", "function c() { return 'c'; }\n"),
  ])).toStringWithSourceMap({
    file: "abc.js",
    sourceRoot: "http://example.com/www/js/"
  });

  code += "//# sourceMappingURL=data:text/json;base64," + btoa(map.toString());

  Components.utils.evalInSandbox(code, debuggee, "1.8",
                                 "http://example.com/www/js/abc.js", 1);
}
