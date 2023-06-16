"use strict";

const SOURCE_URL = getFileUrl("setBreakpoint-on-line-in-gcd-script.js");

add_task(
  threadFrontTest(
    async ({ threadFront, debuggee, targetFront }) => {
      const promise = waitForNewSource(threadFront, SOURCE_URL);
      loadSubScriptWithOptions(SOURCE_URL, {
        target: debuggee,
        ignoreCache: true,
      });
      Cu.forceGC();
      Cu.forceGC();
      Cu.forceGC();

      const { source } = await promise;

      const location = { sourceUrl: source.url, line: 7 };
      setBreakpoint(threadFront, location);

      const packet = await executeOnNextTickAndWaitForPause(function () {
        reload(targetFront).then(function () {
          loadSubScriptWithOptions(SOURCE_URL, {
            target: debuggee,
            ignoreCache: true,
          });
        });
      }, threadFront);
      const why = packet.why;
      const environment = await packet.frame.getEnvironment();
      Assert.equal(why.type, "breakpoint");
      Assert.equal(why.actors.length, 1);
      const frame = packet.frame;
      const where = frame.where;
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
