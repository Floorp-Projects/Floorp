"use strict";

var SOURCE_URL = getFileUrl("setBreakpoint-on-column-with-no-offsets-at-end-of-line.js");

function run_test() {
  return Task.spawn(function* () {
    do_test_pending();

    DebuggerServer.registerModule("xpcshell-test/testactors");
    DebuggerServer.init(() => true);

    let global = createTestGlobal("test");
    DebuggerServer.addTestGlobal(global);

    let client = new DebuggerClient(DebuggerServer.connectPipe());
    yield connect(client);

    let { tabs } = yield listTabs(client);
    let tab = findTab(tabs, "test");
    let [, tabClient] = yield attachTab(client, tab);
    let [, threadClient] = yield attachThread(tabClient);
    yield resume(threadClient);

    let promise = waitForNewSource(threadClient, SOURCE_URL);
    loadSubScript(SOURCE_URL, global);
    let { source } = yield promise;
    let sourceClient = threadClient.source(source);

    let location = { line: 4, column: 23 };
    let [packet, breakpointClient] = yield setBreakpoint(sourceClient, location);
    do_check_true(packet.isPending);
    do_check_false("actualLocation" in packet);

    Cu.evalInSandbox("f()", global);
    yield close(client);

    do_test_finished();
  });
}
