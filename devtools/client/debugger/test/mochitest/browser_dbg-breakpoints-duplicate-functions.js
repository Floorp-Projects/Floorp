/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

function firstBreakpoint(dbg) {
  return dbg.selectors.getBreakpointsList()[0];
}

// tests to make sure we do not accidentally slide the breakpoint up to the first
// function with the same name in the file.
add_task(async function() {
  const dbg = await initDebugger(
    "doc-duplicate-functions.html",
    "doc-duplicate-functions"
  );
  const source = findSource(dbg, "doc-duplicate-functions");

  await selectSource(dbg, source.url);
  await addBreakpoint(dbg, source.url, 19);

  await reload(dbg, source.url);
  await waitForState(dbg, state => dbg.selectors.getBreakpointCount() == 1);
  is(firstBreakpoint(dbg).location.line, 19, "Breakpoint is on line 19");
});
