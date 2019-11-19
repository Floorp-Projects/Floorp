"use strict";

const SOURCE_URL = getFileUrl("setBreakpoint-on-column.js");

add_task(
  threadFrontTest(
    async ({ threadFront, debuggee }) => {
      const promise = waitForNewSource(threadFront, SOURCE_URL);
      loadSubScript(SOURCE_URL, debuggee);
      const { source } = await promise;

      const location = { sourceUrl: source.url, line: 4, column: 21 };
      setBreakpoint(threadFront, location);

      const packet = await executeOnNextTickAndWaitForPause(function() {
        Cu.evalInSandbox("f()", debuggee);
      }, threadFront);
      const why = packet.why;
      Assert.equal(why.type, "breakpoint");
      Assert.equal(why.actors.length, 1);
      const frame = packet.frame;
      const where = frame.where;
      Assert.equal(where.actor, source.actor);
      Assert.equal(where.line, location.line);
      Assert.equal(where.column, location.column);
      const variables = frame.environment.bindings.variables;
      Assert.equal(variables.a.value, 1);
      Assert.equal(variables.b.value.type, "undefined");
      Assert.equal(variables.c.value.type, "undefined");

      await resume(threadFront);
    },
    { doNotRunWorker: true }
  )
);
