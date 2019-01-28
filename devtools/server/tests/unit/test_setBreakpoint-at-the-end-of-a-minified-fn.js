"use strict";

const SOURCE_URL = getFileUrl("setBreakpoint-on-column-minified.js");

add_task(threadClientTest(async ({ threadClient, debuggee, client }) => {
  const promise = waitForNewSource(threadClient, SOURCE_URL);
  loadSubScript(SOURCE_URL, debuggee);
  const { source } = await promise;
  const sourceClient = threadClient.source(source);

  // Pause inside of the nested function so we can make sure that we don't
  // add any other breakpoints at other places on this line.
  const location = { line: 3, column: 81 };
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
  const where = frame.where;
  Assert.equal(where.actor, source.actor);
  Assert.equal(where.line, location.line);
  Assert.equal(where.column, 81);

  const variables = frame.environment.bindings.variables;
  Assert.equal(variables.a.value, 1);
  Assert.equal(variables.b.value, 2);
  Assert.equal(variables.c.value, 3);

  await resume(threadClient);
}, { doNotRunWorker: true }));
