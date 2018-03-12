"use strict";

var SOURCE_URL = getFileUrl("setBreakpoint-on-column-in-gcd-script.js");

function run_test() {
  return (async function() {
    do_test_pending();

    let global = testGlobal("test");
    loadSubScriptWithOptions(SOURCE_URL, {target: global, ignoreCache: true});
    Cu.forceGC(); Cu.forceGC(); Cu.forceGC();

    DebuggerServer.registerModule("xpcshell-test/testactors");
    DebuggerServer.init(() => true);
    DebuggerServer.addTestGlobal(global);
    let client = new DebuggerClient(DebuggerServer.connectPipe());
    await connect(client);

    let { tabs } = await listTabs(client);
    let tab = findTab(tabs, "test");
    let [, tabClient] = await attachTab(client, tab);
    let [, threadClient] = await attachThread(tabClient);
    await resume(threadClient);

    let { sources } = await getSources(threadClient);
    let source = findSource(sources, SOURCE_URL);
    let sourceClient = threadClient.source(source);

    let location = { line: 6, column: 17 };
    let [packet, breakpointClient] = await setBreakpoint(sourceClient, location);
    Assert.ok(packet.isPending);
    Assert.equal(false, "actualLocation" in packet);

    packet = await executeOnNextTickAndWaitForPause(function() {
      reload(tabClient).then(function() {
        loadSubScriptWithOptions(SOURCE_URL, {target: global, ignoreCache: true});
      });
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
    Assert.equal(where.column, location.column);
    let variables = frame.environment.bindings.variables;
    Assert.equal(variables.a.value, 1);
    Assert.equal(variables.b.value.type, "undefined");
    Assert.equal(variables.c.value.type, "undefined");
    await resume(threadClient);

    await close(client);
    do_test_finished();
  })();
}
