/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that we don't permanently cache source maps across reloads.
 */

var gDebuggee;
var gClient;
var gThreadClient;
var gTabClient;

const {SourceNode} = require("source-map");

function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-source-map");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient, "test-source-map",
                           function(response, tabClient, threadClient) {
                             gThreadClient = threadClient;
                             gTabClient = tabClient;
                             setup_code();
                           });
  });
  do_test_pending();
}

// The MAP_FILE_NAME is .txt so that the OS will definitely have an extension ->
// content type mapping for the extension. If it doesn't (like .map or .json),
// it logs console errors, which cause the test to fail. See bug 907839.
const MAP_FILE_NAME = "temporary-generated.txt";

const TEMP_FILE_1 = "temporary1.js";
const TEMP_FILE_2 = "temporary2.js";
const TEMP_GENERATED_SOURCE = "temporary-generated.js";

function setup_code() {
  const node = new SourceNode(1, 0,
                            getFileUrl(TEMP_FILE_1, true),
                            "function temporary1() {}\n");
  let { code, map } = node.toStringWithSourceMap({
    file: getFileUrl(TEMP_GENERATED_SOURCE, true)
  });

  code += "//# sourceMappingURL=" + getFileUrl(MAP_FILE_NAME, true);
  writeFile(MAP_FILE_NAME, map.toString());

  Cu.evalInSandbox(code,
                   gDebuggee,
                   "1.8",
                   getFileUrl(TEMP_GENERATED_SOURCE, true),
                   1);

  test_initial_sources();
}

function test_initial_sources() {
  gThreadClient.getSources(function({ error, sources }) {
    Assert.ok(!error);
    sources = sources.filter(source => source.url);
    Assert.equal(sources.length, 1);
    Assert.equal(sources[0].url, getFileUrl(TEMP_FILE_1, true));
    reload(gTabClient).then(setup_new_code);
  });
}

function setup_new_code() {
  const node = new SourceNode(1, 0,
                            getFileUrl(TEMP_FILE_2, true),
                            "function temporary2() {}\n");
  let { code, map } = node.toStringWithSourceMap({
    file: getFileUrl(TEMP_GENERATED_SOURCE, true)
  });

  code += "\n//# sourceMappingURL=" + getFileUrl(MAP_FILE_NAME, true);
  writeFile(MAP_FILE_NAME, map.toString());

  gThreadClient.addOneTimeListener("newSource", test_new_sources);
  Cu.evalInSandbox(code,
                   gDebuggee,
                   "1.8",
                   getFileUrl(TEMP_GENERATED_SOURCE, true),
                   1);
}

function test_new_sources() {
  gThreadClient.getSources(function({ error, sources }) {
    Assert.ok(!error);
    sources = sources.filter(source => source.url);

    // Should now have TEMP_FILE_2 as a source.
    Assert.equal(sources.length, 1);
    const s = sources.filter(source => source.url === getFileUrl(TEMP_FILE_2, true))[0];
    Assert.ok(!!s);

    finish_test();
  });
}

function finish_test() {
  do_get_file(MAP_FILE_NAME).remove(false);
  finishClient(gClient);
}
