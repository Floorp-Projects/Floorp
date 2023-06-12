/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that makes sure web console eval happens in the user-selected stackframe
// from the js debugger.

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-eval-in-stackframe.html";

add_task(async function () {
  // TODO: Remove this pref change when middleware for terminating requests
  // when closing a panel is implemented
  await pushPref("devtools.debugger.features.inline-preview", false);

  info("open the console");
  const hud = await openNewTabAndConsole(TEST_URI);

  info("Check `foo` value");
  await executeAndWaitForResultMessage(hud, "foo", "globalFooBug783499");
  ok(true, "|foo| value is correct");

  info("Assign and check `foo2` value");
  await executeAndWaitForResultMessage(
    hud,
    "foo2 = 'newFoo'; window.foo2",
    "newFoo"
  );
  ok(true, "'newFoo' is displayed after adding `foo2`");

  info("Open the debugger and then select the console again");
  await openDebugger();
  const toolbox = hud.toolbox;
  const dbg = createDebuggerContext(toolbox);

  await openConsole();

  info("Check `foo + foo2` value");
  await executeAndWaitForResultMessage(
    hud,
    "foo + foo2",
    "globalFooBug783499newFoo"
  );

  info("Select the debugger again");
  await openDebugger();
  await pauseDebugger(dbg);

  const stackFrames = dbg.selectors.getCurrentThreadFrames();

  info("frames added, select the console again");
  await openConsole();

  info("Check `foo + foo2` value when paused");
  await executeAndWaitForResultMessage(
    hud,
    "foo + foo2",
    "globalFooBug783499foo2SecondCall"
  );
  ok(true, "`foo + foo2` from `secondCall()`");

  info("select the debugger and select the frame (1)");
  await openDebugger();

  await selectFrame(dbg, stackFrames[1]);

  await openConsole();

  info("Check `foo + foo2 + foo3` value when paused on a given frame");
  await executeAndWaitForResultMessage(
    hud,
    "foo + foo2 + foo3",
    "fooFirstCallnewFoofoo3FirstCall"
  );
  ok(true, "`foo + foo2 + foo3` from `firstCall()`");

  await executeAndWaitForResultMessage(
    hud,
    "foo = 'abba'; foo3 = 'bug783499'; foo + foo3",
    "abbabug783499"
  );
  ok(true, "`foo + foo3` updated in `firstCall()`");

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    is(
      content.wrappedJSObject.foo,
      "globalFooBug783499",
      "`foo` in content window"
    );
    is(content.wrappedJSObject.foo2, "newFoo", "`foo2` in content window");
    ok(
      !content.wrappedJSObject.foo3,
      "`foo3` was not added to the content window"
    );
  });
  await resume(dbg);

  info(
    "Check executing expression with private properties access while paused in class method"
  );
  const onPaused = waitForPaused(dbg);
  // breakFn has a debugger statement that will pause the debugger
  execute(hud, `x = new Foo(); x.breakFn()`);
  await onPaused;
  // pausing opens the debugger, switch to the console again
  await openConsole();

  await executeAndWaitForResultMessage(
    hud,
    "this.#privateProp",
    "privatePropValue"
  );
  ok(
    true,
    "evaluating a private properties while paused in a class method does work"
  );

  await executeAndWaitForResultMessage(
    hud,
    "Foo.#privateStatic",
    `Object { first: "a", second: "b" }`
  );
  ok(
    true,
    "evaluating a static private properties while paused in a class method does work"
  );

  await resume(dbg);
});
