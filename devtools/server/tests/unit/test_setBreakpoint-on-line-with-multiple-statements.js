"use strict";

var SOURCE_URL = getFileUrl("setBreakpoint-on-line-with-multiple-statements.js");

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

    let location = { line: 4 };
    let [packet, breakpointClient] = yield setBreakpoint(sourceClient, location);
    do_check_false(packet.isPending);
    do_check_false("actualLocation" in packet);

    packet = yield executeOnNextTickAndWaitForPause(function () {
      Cu.evalInSandbox("f()", global);
    }, client);
    do_check_eq(packet.type, "paused");
    let why = packet.why;
    do_check_eq(why.type, "breakpoint");
    do_check_eq(why.actors.length, 1);
    do_check_eq(why.actors[0], breakpointClient.actor);
    let frame = packet.frame;
    let where = frame.where;
    do_check_eq(where.source.actor, source.actor);
    do_check_eq(where.line, location.line);
    let variables = frame.environment.bindings.variables;
    do_check_eq(variables.a.value.type, "undefined");
    do_check_eq(variables.b.value.type, "undefined");
    do_check_eq(variables.c.value.type, "undefined");

    yield resume(threadClient);
    yield close(client);

    do_test_finished();
  });
}
