"use strict";

const SOURCE_URL = getFileUrl("setBreakpoint-on-line-with-multiple-offsets.js");

add_task(threadClientTest(async ({ threadClient, debuggee, client }) => {
  const promise = waitForNewSource(threadClient, SOURCE_URL);
  loadSubScript(SOURCE_URL, debuggee);
  const { source } = await promise;
  const sourceClient = threadClient.source(source);

  const location = { line: 4 };
  let [packet, breakpointClient] = await setBreakpoint(sourceClient, location);
  Assert.ok(!packet.isPending);
  Assert.equal(false, "actualLocation" in packet);

  packet = await executeOnNextTickAndWaitForPause(function() {
    Cu.evalInSandbox("f()", debuggee);
  }, client);
  Assert.equal(packet.type, "paused");
  let why = packet.why;
  Assert.equal(why.type, "breakpoint");
  Assert.equal(why.actors.length, 1);
  Assert.equal(why.actors[0], breakpointClient.actor);
  let frame = packet.frame;
  let where = frame.where;
  Assert.equal(where.actor, source.actor);
  Assert.equal(where.line, location.line);
  let variables = frame.environment.bindings.variables;
  Assert.equal(variables.i.value.type, "undefined");

  packet = await executeOnNextTickAndWaitForPause(
    () => resume(threadClient),
    client
  );
  Assert.equal(packet.type, "paused");
  why = packet.why;
  Assert.equal(why.type, "breakpoint");
  Assert.equal(why.actors.length, 1);
  Assert.equal(why.actors[0], breakpointClient.actor);
  frame = packet.frame;
  where = frame.where;
  Assert.equal(where.actor, source.actor);
  Assert.equal(where.line, location.line);
  variables = frame.environment.bindings.variables;
  Assert.equal(variables.i.value, 0);

  await resume(threadClient);
}, { doNotRunWorker: true }));
