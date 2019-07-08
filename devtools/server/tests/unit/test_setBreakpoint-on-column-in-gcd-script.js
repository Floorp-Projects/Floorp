"use strict";

const SOURCE_URL = getFileUrl("setBreakpoint-on-column-in-gcd-script.js");

add_task(
  threadClientTest(
    async ({ threadClient, debuggee, targetFront }) => {
      const promise = waitForNewSource(threadClient, SOURCE_URL);
      loadSubScriptWithOptions(SOURCE_URL, {
        target: debuggee,
        ignoreCache: true,
      });
      Cu.forceGC();
      Cu.forceGC();
      Cu.forceGC();

      const { source } = await promise;

      const location = { sourceUrl: source.url, line: 6, column: 21 };
      setBreakpoint(threadClient, location);

      const packet = await executeOnNextTickAndWaitForPause(function() {
        reload(targetFront).then(function() {
          loadSubScriptWithOptions(SOURCE_URL, {
            target: debuggee,
            ignoreCache: true,
          });
        });
      }, threadClient);
      const why = packet.why;
      Assert.equal(why.type, "breakpoint");
      Assert.equal(why.actors.length, 1);
      const frame = packet.frame;
      const where = frame.where;
      Assert.equal(where.line, location.line);
      Assert.equal(where.column, location.column);
      const variables = frame.environment.bindings.variables;
      Assert.equal(variables.a.value, 1);
      Assert.equal(variables.b.value.type, "undefined");
      Assert.equal(variables.c.value.type, "undefined");
      await resume(threadClient);
    },
    { doNotRunWorker: true }
  )
);
