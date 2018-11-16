/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that a basic XHR breakpoint works for get and POST is ignored
add_task(async function() {
    const dbg = await initDebugger("doc-xhr.html");
    await waitForSources(dbg, "fetch.js");
    await dbg.actions.setXHRBreakpoint("doc", "GET");
    invokeInTab("main", "doc-xhr.html");
    await waitForPaused(dbg);
    assertPausedLocation(dbg);
    resume(dbg);

    await dbg.actions.removeXHRBreakpoint(0);
    invokeInTab("main", "doc-xhr.html");
    assertNotPaused(dbg);

    await dbg.actions.setXHRBreakpoint("doc-xhr.html", "POST");
    invokeInTab("main", "doc");
    assertNotPaused(dbg);
});

// Tests the "pause on any URL" checkbox works properly
add_task(async function() {
    const dbg = await initDebugger("doc-xhr.html");
    await waitForSources(dbg, "fetch.js");

    // Enable pause on any URL
    await dbg.actions.togglePauseOnAny();
    invokeInTab("main", "doc-xhr.html");
    await waitForPaused(dbg);
    await resume(dbg);

    invokeInTab("main", "fetch.js");
    await waitForPaused(dbg);
    await resume(dbg);

    invokeInTab("main", "README.md");
    await waitForPaused(dbg);
    await resume(dbg);

    // Disable pause on any URL
    await dbg.actions.togglePauseOnAny();
    invokeInTab("main", "README.md");
    assertNotPaused(dbg);
});
