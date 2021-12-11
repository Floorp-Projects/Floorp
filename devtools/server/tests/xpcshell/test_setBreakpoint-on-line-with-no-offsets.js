"use strict";

const SOURCE_URL = getFileUrl("setBreakpoint-on-line-with-no-offsets.js");

add_task(
  threadFrontTest(
    async ({ threadFront, debuggee }) => {
      const promise = waitForNewSource(threadFront, SOURCE_URL);
      loadSubScript(SOURCE_URL, debuggee);
      const { source } = await promise;
      const sourceFront = threadFront.source(source);

      const location = { line: 5 };
      let [packet, breakpointClient] = await setBreakpoint(
        sourceFront,
        location
      );
      Assert.ok(!packet.isPending);
      Assert.ok("actualLocation" in packet);
      const actualLocation = packet.actualLocation;
      Assert.equal(actualLocation.line, 6);

      packet = await executeOnNextTickAndWaitForPause(function() {
        Cu.evalInSandbox("f()", debuggee);
      }, threadFront);
      const environment = await packet.frame.getEnvironment();
      Assert.equal(packet.type, "paused");
      const why = packet.why;
      Assert.equal(why.type, "breakpoint");
      Assert.equal(why.actors.length, 1);
      Assert.equal(why.actors[0], breakpointClient.actor);
      const frame = packet.frame;
      const where = frame.where;
      Assert.equal(where.actor, source.actor);
      Assert.equal(where.line, actualLocation.line);
      const variables = environment.bindings.variables;
      Assert.equal(variables.a.value, 1);
      Assert.equal(variables.c.value.type, "undefined");

      await resume(threadFront);
    },
    { doNotRunWorker: true }
  )
);
