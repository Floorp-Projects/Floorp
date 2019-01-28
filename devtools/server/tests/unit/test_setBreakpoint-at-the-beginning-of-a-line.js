"use strict";

const SOURCE_URL = getFileUrl("setBreakpoint-on-column.js");

add_task(threadClientTest(async ({ threadClient, debuggee, client }) => {
  const promise = waitForNewSource(threadClient, SOURCE_URL);
  loadSubScript(SOURCE_URL, debuggee);
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
    Cu.evalInSandbox("f()", debuggee);
  }, client);

  Assert.equal(packet.type, "paused");
  const why = packet.why;
  Assert.equal(why.type, "breakpoint");
  Assert.equal(why.actors.length, 1);
  Assert.equal(why.actors[0], breakpointClient.actor);

  const frame = packet.frame;
  Assert.equal(frame.where.actor, source.actor);
  Assert.equal(frame.where.line, location.line);
  Assert.equal(frame.where.column, 6);

  const variables = frame.environment.bindings.variables;
  Assert.equal(variables.a.value.type, "undefined");
  Assert.equal(variables.b.value.type, "undefined");
  Assert.equal(variables.c.value.type, "undefined");

  await resume(threadClient);
}, { doNotRunWorker: true }));
