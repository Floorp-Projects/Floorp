"use strict";

const SOURCE_URL = getFileUrl("setBreakpoint-on-line-with-no-offsets-in-gcd-script.js");

add_task(threadClientTest(async ({ threadClient, debuggee, client, targetFront }) => {
  const promise = waitForNewSource(threadClient, SOURCE_URL);
  loadSubScriptWithOptions(SOURCE_URL, {target: debuggee, ignoreCache: true});
  Cu.forceGC(); Cu.forceGC(); Cu.forceGC();

  const { source } = await promise;
  const sourceFront = threadClient.source(source);

  const location = { line: 7 };
  let [packet, breakpointClient] = await setBreakpoint(sourceFront, location);
  Assert.ok(packet.isPending);
  Assert.equal(false, "actualLocation" in packet);

  packet = await executeOnNextTickAndWaitForPause(function() {
    reload(targetFront).then(function() {
      loadSubScriptWithOptions(SOURCE_URL, {target: debuggee, ignoreCache: true});
    });
  }, client);
  Assert.equal(packet.type, "paused");
  const why = packet.why;
  Assert.equal(why.type, "breakpoint");
  Assert.equal(why.actors.length, 1);
  Assert.equal(why.actors[0], breakpointClient.actor);
  const frame = packet.frame;
  const where = frame.where;
  Assert.equal(where.actor, source.actor);
  Assert.equal(where.line, 8);
  const variables = frame.environment.bindings.variables;
  Assert.equal(variables.a.value, 1);
  Assert.equal(variables.c.value.type, "undefined");

  await resume(threadClient);
}, { doNotRunWorker: true }));
