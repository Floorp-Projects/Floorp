/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// This tests breakpoints resyncing when source content changes
// after reload.
// IMPORTANT NOTE: This test currently test scenarios with breakpoint shifting
// feature enabled and disabled. The breakpoint shifting feature is broken and
// causes failures for the expected behaviours. This feature is going to be
// removed in which case the correct behaviours should begin to pass. To see the
// failures with breakpoints shifting make flag IS_BREAKPOINT_SHIFTING_ENABLED = false

"use strict";

const IS_BREAKPOINT_SHIFTING_ENABLED = true;

const httpServer = createTestHTTPServer();
httpServer.registerContentType("html", "text/html");
httpServer.registerContentType("js", "application/javascript");

httpServer.registerPathHandler(
  "/doc-breakpoint-reload.html",
  (request, response) => {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write(`<!DOCTYPE html>
    <html>
      <head>
        <script type="text/javascript" src="/script.js"></script>
      </head>
    </html>
  `);
  }
);

let views = 0;
httpServer.registerPathHandler("/script.js", (request, response) => {
  response.setHeader("Content-Type", "application/javascript");
  // The script contents to serve on reload of script.js. Each relaod
  // cycles through the script content.
  const content = [
    // CONTENT 1: Source content with 1 function
    // The breakpoint will be set on line 3 (i.e with the `return` statement)
    `function bar() {
  const prefix = "long";
  return prefix + "bar";
}
console.log(bar());`,

    // CONTENT 2: Source content with 2 functions, where the breakpoint is now in a
    // different function though the line does not change.
    `function foo() {
  const prefix = "long";
  return prefix + "foo";
}

function bar() {
  const prefix = "long";
  return prefix + "bar";
}
console.log(bar(), foo());`,

    // CONTENT 3: Source content with comments and 1 function, where the breakpoint is
    // is now at a line with comments (non-breakable line).
    `// This is a random comment which is here
// to move the function a couple of lines
// down, making sure the breakpoint is on
// a non-breakable line.
function bar() {
  const prefix = "long";
  return prefix + "bar";
}
console.log(bar());`,

    // CONTENT 4: Source content with just a comment where the line which the breakpoint
    // is supposed to be on no longer exists.
    `// one line comment`,
  ];

  response.write(content[views % content.length]);
  views++;
});

const BASE_URL = `http://localhost:${httpServer.identity.primaryPort}/`;

add_task(async function testBreakpointInFunctionRelocation() {
  info("Start test for relocation of breakpoint set in a function");
  const dbg = await initDebuggerWithAbsoluteURL(
    `${BASE_URL}doc-breakpoint-reload.html`,
    "script.js"
  );

  let source = findSource(dbg, "script.js");
  await selectSource(dbg, source);

  info("Add breakpoint in bar()");
  await addBreakpoint(dbg, source, 3);

  info(
    "Assert the text content on line 3 to make sure the breakpoint was set in bar()"
  );
  assertTextContentOnLine(dbg, 3, 'return prefix + "bar";');

  info("Check that only one breakpoint is set");
  is(dbg.selectors.getBreakpointCount(), 1, "Only one breakpoint exists");

  info("Check that only one breakpoint exist on the server");
  is(
    dbg.client.getServerBreakpointsList().length,
    1,
    "Only one breakpoint exists on the server"
  );

  info(
    "Reload should change the source content to CONTENT 2 i.e 2 functions foo() and bar()"
  );
  await reload(dbg);
  await waitForPaused(dbg);

  source = findSource(dbg, "script.js");

  info("Assert that the breakpoint pauses on line 3");
  assertPausedAtSourceAndLine(dbg, source.id, 3);

  if (IS_BREAKPOINT_SHIFTING_ENABLED) {
    // This is one of the bugs we see, the debugger pauses
    // on line 3 while the breakpoint has move to line 8
    info("Assert that the breakpoint is visible on line 8");
    await assertBreakpoint(dbg, 8);
  } else {
    info("Assert that the breakpoint is visible on line 3");
    await assertBreakpoint(dbg, 3);
  }

  info("Assert the text content on line 3 to make sure we are paused in foo()");
  assertTextContentOnLine(dbg, 3, 'return prefix + "foo";');

  info("Check that only one breakpoint currently exists");
  is(dbg.selectors.getBreakpointCount(), 1, "One breakpoint exists");

  info("Check that only one breakpoint exist on the server");
  is(
    dbg.client.getServerBreakpointsList().length,
    1,
    "Only one breakpoint exists on the server"
  );

  await resume(dbg);

  info(
    "Reload should change the source content to CONTENT 3 i.e comments and 1 function bar()"
  );
  await reload(dbg);
  await waitForSelectedSource(dbg, "script.js");

  if (IS_BREAKPOINT_SHIFTING_ENABLED) {
    // Another bug here is that this should be paused as the
    // breakpoint shifted to line 7 which is a breakble line
    assertNotPaused(dbg);

    info("Assert that the breakpoint is visible on line 7");
    await assertBreakpoint(dbg, 7);
  } else {
    assertNotPaused(dbg);

    info(
      "Assert that the breakpoint is still visible on line 3 which is a non-breakable line"
    );
    await assertBreakpoint(dbg, 3);
  }

  info(
    "Assert the text content on line 3 to make sure the breakpoint is set line 3 of the comment"
  );
  assertTextContentOnLine(dbg, 3, "// down, making sure the breakpoint is on");

  info("Check that only one breakpoint still exists");
  is(dbg.selectors.getBreakpointCount(), 1, "Only one breakpoint exists");

  info("Check that one breakpoint exist on the server");
  is(
    dbg.client.getServerBreakpointsList().length,
    1,
    "One breakpoint exists on the server"
  );

  info(
    "Reload should change the source content to CONTENT 4 which is just a one comment line"
  );
  await reload(dbg);
  await waitForSelectedSource(dbg, "script.js");

  // There will initially be zero breakpoints, but wait to make sure none are
  // installed while syncing.
  await wait(1000);
  assertNotPaused(dbg);

  info("Assert that the source content is one comment line");
  assertTextContentOnLine(dbg, 1, "// one line comment");

  info("Check that the breakpoint has been removed");
  is(dbg.selectors.getBreakpointCount(), 0, "No breakpoint exists");

  // Breakpoints do not get removed in this situation, to support breakpoints in
  // inline sources which load and get hit progressively. See the browser_dbg-breakpoints-reloading.js
  // test for this behaviour.
  info("Check that one breakpoint exists on the server");
  is(
    dbg.client.getServerBreakpointsList().length,
    1,
    "One breakpoint exists on the server"
  );
});
