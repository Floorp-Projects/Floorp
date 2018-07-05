"use strict";

var SOURCE_URL = getFileUrl("setBreakpoint-on-line.js");

function run_test() {
  return (async function() {
    do_test_pending();

    const { createRootActor } = require("xpcshell-test/testactors");
    DebuggerServer.setRootActor(createRootActor);
    DebuggerServer.init(() => true);

    const global = createTestGlobal("test");
    DebuggerServer.addTestGlobal(global);

    const client = new DebuggerClient(DebuggerServer.connectPipe());
    await connect(client);

    const { tabs } = await listTabs(client);
    const tab = findTab(tabs, "test");
    const [, tabClient] = await attachTab(client, tab);

    const [, threadClient] = await attachThread(tabClient);
    await resume(threadClient);

    const promise = waitForNewSource(threadClient, SOURCE_URL);
    loadSubScript(SOURCE_URL, global);
    const { source } = await promise;
    const sourceClient = threadClient.source(source);

    const location = { line: 5 };
    let [packet, breakpointClient] = await setBreakpoint(sourceClient, location);
    Assert.ok(!packet.isPending);
    Assert.equal(false, "actualLocation" in packet);

    packet = await executeOnNextTickAndWaitForPause(function() {
      Cu.evalInSandbox("f()", global);
    }, client);
    Assert.equal(packet.type, "paused");
    const why = packet.why;
    Assert.equal(why.type, "breakpoint");
    Assert.equal(why.actors.length, 1);
    Assert.equal(why.actors[0], breakpointClient.actor);
    const frame = packet.frame;
    const where = frame.where;
    Assert.equal(where.source.actor, source.actor);
    Assert.equal(where.line, location.line);
    const variables = frame.environment.bindings.variables;
    Assert.equal(variables.a.value, 1);
    Assert.equal(variables.b.value.type, "undefined");
    Assert.equal(variables.c.value.type, "undefined");

    await resume(threadClient);
    await close(client);

    do_test_finished();
  })();
}
