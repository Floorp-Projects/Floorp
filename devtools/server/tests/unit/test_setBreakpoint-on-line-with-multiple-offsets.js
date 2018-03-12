"use strict";

var SOURCE_URL = getFileUrl("setBreakpoint-on-line-with-multiple-offsets.js");

function run_test() {
  return (async function() {
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

    let location = { line: 4 };
    let [packet, breakpointClient] = await setBreakpoint(sourceClient, location);
    Assert.ok(!packet.isPending);
    Assert.equal(false, "actualLocation" in packet);

    packet = await executeOnNextTickAndWaitForPause(function() {
      Cu.evalInSandbox("f()", global);
    }, client);
    Assert.equal(packet.type, "paused");
    let why = packet.why;
    Assert.equal(why.type, "breakpoint");
    Assert.equal(why.actors.length, 1);
    Assert.equal(why.actors[0], breakpointClient.actor);
    let frame = packet.frame;
    let where = frame.where;
    Assert.equal(where.source.actor, source.actor);
    Assert.equal(where.line, location.line);
    let variables = frame.environment.bindings.variables;
    Assert.equal(variables.i.value.type, "undefined");

    packet = await executeOnNextTickAndWaitForPause(function() {
      resume(threadClient);
    }, client);
    Assert.equal(packet.type, "paused");
    why = packet.why;
    Assert.equal(why.type, "breakpoint");
    Assert.equal(why.actors.length, 1);
    Assert.equal(why.actors[0], breakpointClient.actor);
    frame = packet.frame;
    where = frame.where;
    Assert.equal(where.source.actor, source.actor);
    Assert.equal(where.line, location.line);
    variables = frame.environment.bindings.variables;
    Assert.equal(variables.i.value, 0);

    await resume(threadClient);
    await close(client);

    do_test_finished();
  })();
}
