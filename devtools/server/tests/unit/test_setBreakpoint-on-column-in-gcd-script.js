"use strict";

var SOURCE_URL = getFileUrl("setBreakpoint-on-column-in-gcd-script.js");

function run_test() {
  return Task.spawn(function* () {
    do_test_pending();

    let global = testGlobal("test");
    loadSubScriptWithOptions(SOURCE_URL, {target: global, ignoreCache: true});
    Cu.forceGC(); Cu.forceGC(); Cu.forceGC();

    DebuggerServer.registerModule("xpcshell-test/testactors");
    DebuggerServer.init(() => true);
    DebuggerServer.addTestGlobal(global);
    let client = new DebuggerClient(DebuggerServer.connectPipe());
    yield connect(client);

    let { tabs } = yield listTabs(client);
    let tab = findTab(tabs, "test");
    let [, tabClient] = yield attachTab(client, tab);
    let [, threadClient] = yield attachThread(tabClient);
    yield resume(threadClient);

    let { sources } = yield getSources(threadClient);
    let source = findSource(sources, SOURCE_URL);
    let sourceClient = threadClient.source(source);

    let location = { line: 6, column: 17 };
    let [packet, breakpointClient] = yield setBreakpoint(sourceClient, location);
    Assert.ok(packet.isPending);
    Assert.equal(false, "actualLocation" in packet);

    packet = yield executeOnNextTickAndWaitForPause(function () {
      reload(tabClient).then(function () {
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
    yield resume(threadClient);

    yield close(client);
    do_test_finished();
  });
}
