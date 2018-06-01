"use strict";

var SOURCE_URL = getFileUrl("setBreakpoint-on-column.js");

async function run_test() {
  do_test_pending();
  DebuggerServer.registerModule("xpcshell-test/testactors");
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

  const location = { line: 4, column: 2 };
  let [packet, breakpointClient] = await setBreakpoint(
    sourceClient,
    location
  );

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
  Assert.equal(where.column, 6);

  const variables = frame.environment.bindings.variables;
  Assert.equal(variables.a.value.type, "undefined");
  Assert.equal(variables.b.value.type, "undefined");
  Assert.equal(variables.c.value.type, "undefined");

  await resume(threadClient);
  await close(client);
  do_test_finished();
}
