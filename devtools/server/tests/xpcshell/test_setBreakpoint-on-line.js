"use strict";

const SOURCE_URL = getFileUrl("setBreakpoint-on-line.js");

add_task(
  threadFrontTest(
    async ({ threadFront, debuggee }) => {
      const promise = waitForNewSource(threadFront, SOURCE_URL);
      loadSubScript(SOURCE_URL, debuggee);
      const { source } = await promise;
      const sourceFront = threadFront.source(source);

      const location = { sourceUrl: sourceFront.url, line: 5 };
      setBreakpoint(threadFront, location);

      const packet = await executeOnNextTickAndWaitForPause(function () {
        Cu.evalInSandbox("f()", debuggee);
      }, threadFront);
      const environment = await packet.frame.getEnvironment();
      const why = packet.why;
      Assert.equal(why.type, "breakpoint");
      Assert.equal(why.actors.length, 1);
      const frame = packet.frame;
      const where = frame.where;
      Assert.equal(where.actor, source.actor);
      Assert.equal(where.line, location.line);
      const variables = environment.bindings.variables;
      Assert.equal(variables.a.value, 1);
      Assert.equal(variables.b.value.type, "undefined");
      Assert.equal(variables.c.value.type, "undefined");

      await resume(threadFront);
    },
    { doNotRunWorker: true }
  )
);
