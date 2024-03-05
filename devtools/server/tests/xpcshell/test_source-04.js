/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const SOURCE_URL = getFileUrl("source-03.js");

add_task(
  threadFrontTest(
    async ({ threadFront, server }) => {
      const promise = waitForNewSource(threadFront, SOURCE_URL);

      // Create two globals in the default junk sandbox compartment so that
      // both globals are part of the same compartment.
      server.allowNewThreadGlobals();
      const debuggee1 = Cu.Sandbox(systemPrincipal);
      debuggee1.__name = "debuggee2.js";
      const debuggee2 = Cu.Sandbox(systemPrincipal);
      debuggee2.__name = "debuggee2.js";
      server.disallowNewThreadGlobals();

      // Load first copy of the source file. The first call to "loadSubScript" will
      // create a ScriptSourceObject and a JSScript which references it.
      loadSubScript(SOURCE_URL, debuggee1);

      await promise;

      // We want to set a breakpoint and make sure that the breakpoint is properly
      // set on _both_ files backed
      await setBreakpoint(threadFront, {
        sourceUrl: SOURCE_URL,
        line: 4,
      });

      const { sources } = await getSources(threadFront);
      Assert.equal(sources.length, 1);

      // Ensure that the breakpoint was properly applied to the JSScipt loaded
      // in the first global.
      let pausedOne = false;
      let onResumed = null;
      threadFront.once("paused", function () {
        pausedOne = true;
        onResumed = resume(threadFront);
      });
      Cu.evalInSandbox("init()", debuggee1, "1.8", "test.js", 1);
      await onResumed;
      Assert.equal(pausedOne, true);

      // Load second copy of the source file. The second call will attempt to
      // re-use JSScript objects because that is what loadSubScript does for
      // instances of the same file that are loaded in the system principal in
      // the same compartment.
      //
      // We explicitly want this because it is an edge case of the server. Most
      // of the time a Debugger.Source will only have a single Debugger.Script
      // associated with a given function, but in the context of explicitly
      // cloned JSScripts, this is not the case, and we need to handle that.
      loadSubScript(SOURCE_URL, debuggee2);

      // Ensure that the breakpoint was properly applied to the JSScipt loaded
      // in the second global.
      let pausedTwo = false;
      threadFront.once("paused", function () {
        pausedTwo = true;
        onResumed = resume(threadFront);
      });
      Cu.evalInSandbox("init()", debuggee2, "1.8", "test.js", 1);
      await onResumed;
      Assert.equal(pausedTwo, true);
    },
    { doNotRunWorker: true }
  )
);
