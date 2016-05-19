/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we can black box source mapped sources.
 */

var gDebuggee;
var gClient;
var gThreadClient;

const {SourceNode} = require("source-map");

function run_test()
{
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-black-box");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function () {
    attachTestTabAndResume(gClient, "test-black-box", function (aResponse, aTabClient, aThreadClient) {
      gThreadClient = aThreadClient;

      promise.resolve(setup_code())
        .then(black_box_code)
        .then(run_code)
        .then(test_correct_location)
        .then(null, function (error) {
          do_check_true(false, "Should not get an error, got " + error);
        })
        .then(function () {
          finishClient(gClient);
        });
    });
  });
  do_test_pending();
}

function setup_code() {
  let { code, map } = (new SourceNode(null, null, null, [
    new SourceNode(1, 0, "a.js", "" + function a() {
      return b();
    }),
    "\n",
    new SourceNode(1, 0, "b.js", "" + function b() {
      debugger; // Don't want to stop here.
      return c();
    }),
    "\n",
    new SourceNode(1, 0, "c.js", "" + function c() {
      debugger; // Want to stop here.
    }),
    "\n"
  ])).toStringWithSourceMap({
    file: "abc.js",
    sourceRoot: "http://example.com/"
  });

  code += "//# sourceMappingURL=data:text/json," + map.toString();

  Components.utils.evalInSandbox(code,
                                 gDebuggee,
                                 "1.8",
                                 "http://example.com/abc.js");
}

function black_box_code() {
  const d = promise.defer();

  gThreadClient.getSources(function ({ sources, error }) {
    do_check_true(!error, "Shouldn't get an error getting sources");
    const source = sources.filter((s) => {
      return s.url.indexOf("b.js") !== -1;
    })[0];
    do_check_true(!!source, "We should have our source in the sources list");

    gThreadClient.source(source).blackBox(function ({ error }) {
      do_check_true(!error, "Should not get an error black boxing");
      d.resolve(true);
    });
  });

  return d.promise;
}

function run_code() {
  const d = promise.defer();

  gClient.addOneTimeListener("paused", function (aEvent, aPacket) {
    d.resolve(aPacket);
    gThreadClient.resume();
  });
  gDebuggee.a();

  return d.promise;
}

function test_correct_location(aPacket) {
  do_check_eq(aPacket.why.type, "debuggerStatement",
              "Should hit a debugger statement.");
  do_check_eq(aPacket.frame.where.source.url, "http://example.com/c.js",
              "Should have skipped over the debugger statement in the black boxed source");
}
