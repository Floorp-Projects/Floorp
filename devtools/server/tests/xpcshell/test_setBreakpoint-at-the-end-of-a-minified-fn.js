"use strict";

const SOURCE_URL = getFileUrl("setBreakpoint-on-column-minified.js");

add_task(
  threadFrontTest(
    async ({ threadFront, debuggee }) => {
      const promise = waitForNewSource(threadFront, SOURCE_URL);
      loadSubScript(SOURCE_URL, debuggee);
      const { source } = await promise;

      // Pause inside of the nested function so we can make sure that we don't
      // add any other breakpoints at other places on this line.
      const location = { sourceUrl: source.url, line: 3, column: 81 };
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
      Assert.equal(where.column, 81);

      const environment = await packet.frame.getEnvironment();
      const variables = environment.bindings.variables;
      Assert.equal(variables.a.value, 1);
      Assert.equal(variables.b.value, 2);
      Assert.equal(variables.c.value, 3);

      await resume(threadFront);
    },
    { doNotRunWorker: true }
  )
);
