/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that the editor will always highight the right line, no
// matter if the source text doesn't exist yet or even if the source
// doesn't exist.

add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html", "long.js");
  const {
    selectors: { getSource, getSourceContent }
  } = dbg;
  const sourceUrl = `${EXAMPLE_URL}long.js`;

  // The source itself doesn't even exist yet, and using
  // `selectSourceURL` will set a pending request to load this source
  // and highlight a specific line.

  await selectSource(dbg, sourceUrl, 66);

  // TODO: revisit highlighting lines when the debugger opens
  // assertHighlightLocation(dbg, "long.js", 66);

  log("Select line 16 and make sure the editor scrolled.");
  await selectSource(dbg, "long.js", 16);
  await waitForElementWithSelector(dbg, ".CodeMirror-code > .highlight-line");
  assertHighlightLocation(dbg, "long.js", 16);

  log("Select several locations and check that we have one highlight");
  await selectSource(dbg, "long.js", 17);
  await selectSource(dbg, "long.js", 18);
  assertHighlightLocation(dbg, "long.js", 18);

  // Test jumping to a line in a source that exists but hasn't been
  // loaded yet.
  log("Select an unloaded source");
  selectSource(dbg, "simple1.js", 6);

  // Make sure the source is in the loading state, wait for it to be
  // fully loaded, and check the highlighted line.
  const simple1 = findSource(dbg, "simple1.js");
  is(getSourceContent(simple1.id), null);

  await waitForSelectedSource(dbg, "simple1.js");
  ok(getSourceContent(simple1.id).value.value);
  assertHighlightLocation(dbg, "simple1.js", 6);
});
