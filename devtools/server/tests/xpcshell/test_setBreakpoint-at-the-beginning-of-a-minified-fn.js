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
      const location = { sourceUrl: source.url, line: 3, column: 56 };
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
      Assert.equal(where.column, 56);

      const environment = await packet.frame.getEnvironment();
      const variables = environment.bindings.variables;
      Assert.equal(variables.a.value.type, "undefined");
      Assert.equal(variables.b.value.type, "undefined");
      Assert.equal(variables.c.value.type, "undefined");

      await resume(threadFront);
    },
    { doNotRunWorker: true }
  )
);
