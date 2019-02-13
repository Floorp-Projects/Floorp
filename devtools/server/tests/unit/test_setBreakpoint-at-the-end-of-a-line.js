"use strict";

const SOURCE_URL = getFileUrl("setBreakpoint-on-column.js");

add_task(threadClientTest(async ({ threadClient, debuggee, client }) => {
  const promise = waitForNewSource(threadClient, SOURCE_URL);
  loadSubScript(SOURCE_URL, debuggee);
  const { source } = await promise;

  const location = { sourceUrl: source.url, line: 4, column: 42 };
  setBreakpoint(threadClient, location);

  const packet = await executeOnNextTickAndWaitForPause(function() {
    Cu.evalInSandbox("f()", debuggee);
  }, client);

  Assert.equal(packet.type, "paused");
  const why = packet.why;
  Assert.equal(why.type, "breakpoint");
  Assert.equal(why.actors.length, 1);

  const frame = packet.frame;
  const where = frame.where;
  Assert.equal(where.actor, source.actor);
  Assert.equal(where.line, location.line);
  Assert.equal(where.column, 32);

  const variables = frame.environment.bindings.variables;
  Assert.equal(variables.a.value, 1);
  Assert.equal(variables.b.value, 2);
  Assert.equal(variables.c.value.type, "undefined");

  await resume(threadClient);
}, { doNotRunWorker: true }));
