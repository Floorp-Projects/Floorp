/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Verify that we can load the contents of every source in a source map produced
 * by babel and browserify.
 */

var gDebuggee;
var gClient;

function run_test()
{
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-sourcemaps");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function() {
    attachTestThread(gClient, "test-sourcemaps", testSourcemap);
  });
  do_test_pending();
}

const testSourcemap = Task.async(function*(threadResponse, tabClient, threadClient, tabResponse) {
  evalTestCode();

  const { sources } = yield getSources(threadClient);

  for (let form of sources) {
    let sourceResponse = yield getSourceContent(threadClient.source(form));
    ok(sourceResponse, "Should be able to get the source response");
    ok(sourceResponse.source, "Should have the source text as well");
  }

  finishClient(gClient);
});

const TEST_FILE = "babel_and_browserify_script_with_source_map.js";

function evalTestCode() {
  const testFileContents = readFile(TEST_FILE);
  Cu.evalInSandbox(testFileContents,
                   gDebuggee,
                   "1.8",
                   getFileUrl(TEST_FILE),
                   1);
}
