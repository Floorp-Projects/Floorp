/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test if getExecutableLines return correct information
 */

var gDebuggee;
var gClient;
var gThreadClient;

const SOURCE_MAPPED_FILE = getFileUrl("sourcemapped.js");

function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-get-executable-lines");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function _onConnect() {
    attachTestTabAndResume(
      gClient,
      "test-get-executable-lines",
      function (response, tabClient, threadClient) {
        gThreadClient = threadClient;
        test_executable_lines();
      }
    );
  });

  do_test_pending();
}

function test_executable_lines() {
  gThreadClient.addOneTimeListener("newSource", function _onNewSource(evt, packet) {
    Assert.equal(evt, "newSource");

    gThreadClient.getSources(function ({error, sources}) {
      Assert.ok(!error);
      let source = gThreadClient.source(sources[0]);
      source.getExecutableLines(function (lines) {
        Assert.ok(arrays_equal([1, 2, 4, 6], lines));
        finishClient(gClient);
      });
    });
  });

  let code = readFile("sourcemapped.js") + "\n//# sourceMappingURL=" +
    getFileUrl("source-map-data/sourcemapped.map");

  Components.utils.evalInSandbox(code, gDebuggee, "1.8",
    SOURCE_MAPPED_FILE, 1);
}

function arrays_equal(a, b) {
  return !(a < b || b < a);
}
