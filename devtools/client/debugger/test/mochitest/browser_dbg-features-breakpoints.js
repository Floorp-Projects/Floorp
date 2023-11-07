/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

/**
 * Assert that breakpoints and stepping works in various conditions
 */

const testServer = createVersionizedHttpTestServer(
  "examples/sourcemaps-reload-uncompressed"
);
const TEST_URL = testServer.urlFor("index.html");

add_task(
  async function testSteppingFromOriginalToGeneratedAndAnotherOriginal() {
    const dbg = await initDebuggerWithAbsoluteURL(
      TEST_URL,
      "index.html",
      "script.js",
      "original.js"
    );

    await selectSource(dbg, "original.js");
    await addBreakpoint(dbg, "original.js", 8);
    assertBreakpointSnippet(dbg, 1, "await nonSourceMappedFunction();");

    info("Test pausing on an original source");
    invokeInTab("foo");
    await waitForPausedInOriginalFileAndToggleMapScopes(dbg, "original.js");

    assertPausedAtSourceAndLine(dbg, findSource(dbg, "original.js").id, 8);

    info("Then stepping into a generated source");
    await stepIn(dbg);
    assertPausedAtSourceAndLine(dbg, findSource(dbg, "script.js").id, 5);

    info("Stepping another time within the same generated source");
    await stepIn(dbg);
    assertPausedAtSourceAndLine(dbg, findSource(dbg, "script.js").id, 7);

    info("And finally stepping into another original source");
    await stepIn(dbg);
    assertPausedAtSourceAndLine(
      dbg,
      findSource(dbg, "removed-original.js").id,
      4
    );

    info("Walk up the stack backward, until we resume execution");
    await stepIn(dbg);
    assertPausedAtSourceAndLine(
      dbg,
      findSource(dbg, "removed-original.js").id,
      5
    );

    await stepIn(dbg);
    assertPausedAtSourceAndLine(dbg, findSource(dbg, "script.js").id, 8);

    await stepIn(dbg);
    assertPausedAtSourceAndLine(dbg, findSource(dbg, "original.js").id, 9);

    await stepIn(dbg);
    assertPausedAtSourceAndLine(dbg, findSource(dbg, "original.js").id, 10);

    // We can't use the `stepIn` helper as this last step will resume
    // and the helper is expecting to pause again
    await dbg.actions.stepIn();
    await assertNotPaused(dbg);
  }
);
