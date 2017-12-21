/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/**
 * Test that we can black box source mapped sources.
 */

var gDebuggee;
var gClient;
var gThreadClient;

const {SourceNode} = require("source-map");

function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-black-box");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function () {
    attachTestTabAndResume(
      gClient, "test-black-box",
      function (response, tabClient, threadClient) {
        gThreadClient = threadClient;

        promise.resolve(setup_code())
          .then(black_box_code)
          .then(run_code)
          .then(test_correct_location)
          .catch(function (error) {
            Assert.ok(false, "Should not get an error, got " + error);
          })
          .then(function () {
            finishClient(gClient);
          });
      });
  });
  do_test_pending();
}

function setup_code() {
  /* eslint-disable no-multi-spaces */
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
  /* eslint-enable no-multi-spaces */

  code += "//# sourceMappingURL=data:text/json," + map.toString();

  Components.utils.evalInSandbox(code,
                                 gDebuggee,
                                 "1.8",
                                 "http://example.com/abc.js");
}

function black_box_code() {
  const d = defer();

  gThreadClient.getSources(function ({ sources, error }) {
    Assert.ok(!error, "Shouldn't get an error getting sources");
    const source = sources.filter((s) => {
      return s.url.indexOf("b.js") !== -1;
    })[0];
    Assert.ok(!!source, "We should have our source in the sources list");

    gThreadClient.source(source).blackBox(function ({ error }) {
      Assert.ok(!error, "Should not get an error black boxing");
      d.resolve(true);
    });
  });

  return d.promise;
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

function test_correct_location(packet) {
  Assert.equal(packet.why.type, "debuggerStatement",
               "Should hit a debugger statement.");
  Assert.equal(packet.frame.where.source.url, "http://example.com/c.js",
               "Should have skipped over the debugger statement in the" +
               " black boxed source");
}
