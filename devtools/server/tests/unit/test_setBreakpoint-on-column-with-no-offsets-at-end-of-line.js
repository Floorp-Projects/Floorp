"use strict";

var SOURCE_URL = getFileUrl("setBreakpoint-on-column-with-no-offsets-at-end-of-line.js");

function run_test() {
  return (async function () {
    do_test_pending();

    DebuggerServer.registerModule("xpcshell-test/testactors");
    DebuggerServer.init(() => true);

    let global = createTestGlobal("test");
    DebuggerServer.addTestGlobal(global);

    let client = new DebuggerClient(DebuggerServer.connectPipe());
    await connect(client);

    let { tabs } = await listTabs(client);
    let tab = findTab(tabs, "test");
    let [, tabClient] = await attachTab(client, tab);
    let [, threadClient] = await attachThread(tabClient);
    await resume(threadClient);

    let promise = waitForNewSource(threadClient, SOURCE_URL);
    loadSubScript(SOURCE_URL, global);
    let { source } = await promise;
    let sourceClient = threadClient.source(source);

    let location = { line: 4, column: 23 };
    let [packet, ] = await setBreakpoint(sourceClient, location);
    Assert.ok(packet.isPending);
    Assert.equal(false, "actualLocation" in packet);

    Cu.evalInSandbox("f()", global);
    await close(client);

    do_test_finished();
  })();
}
