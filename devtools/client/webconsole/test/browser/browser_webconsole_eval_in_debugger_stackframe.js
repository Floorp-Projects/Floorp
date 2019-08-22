/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that makes sure web console eval happens in the user-selected stackframe
// from the js debugger.

"use strict";
/* import-globals-from head.js*/

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-eval-in-stackframe.html";

add_task(async function() {
  info("open the console");
  const hud = await openNewTabAndConsole(TEST_URI);

  info("Check `foo` value");
  await executeAndWaitForMessage(hud, "foo", "globalFooBug783499", ".result");
  ok(true, "|foo| value is correct");

  info("Assign and check `foo2` value");
  await executeAndWaitForMessage(
    hud,
    "foo2 = 'newFoo'; window.foo2",
    "newFoo",
    ".result"
  );
  ok(true, "'newFoo' is displayed after adding `foo2`");

  info("Open the debugger and then select the console again");
  await openDebugger();
  const toolbox = hud.toolbox;
  const dbg = createDebuggerContext(toolbox);

  await openConsole();

  info("Check `foo + foo2` value");
  await executeAndWaitForMessage(
    hud,
    "foo + foo2",
    "globalFooBug783499newFoo",
    ".result"
  );

  info("Select the debugger again");
  await openDebugger();
  await pauseDebugger(dbg);

  const stackFrames = dbg.selectors.getCallStackFrames();

  info("frames added, select the console again");
  await openConsole();

  info("Check `foo + foo2` value when paused");
  await executeAndWaitForMessage(
    hud,
    "foo + foo2",
    "globalFooBug783499foo2SecondCall",
    ".result"
  );
  ok(true, "`foo + foo2` from `secondCall()`");

  info("select the debugger and select the frame (1)");
  await openDebugger();

  await selectFrame(dbg, stackFrames[1]);

  await openConsole();

  info("Check `foo + foo2 + foo3` value when paused on a given frame");
  await executeAndWaitForMessage(
    hud,
    "foo + foo2 + foo3",
    "fooFirstCallnewFoofoo3FirstCall",
    ".result"
  );
  ok(true, "`foo + foo2 + foo3` from `firstCall()`");

  await executeAndWaitForMessage(
    hud,
    "foo = 'abba'; foo3 = 'bug783499'; foo + foo3",
    "abbabug783499",
    ".result"
  );
  ok(true, "`foo + foo3` updated in `firstCall()`");

  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
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
});
