/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the editor will always highight the right line, no
// matter if the source text doesn't exist yet or even if the source
// doesn't exist.

add_task(function*() {
  const dbg = yield initDebugger("doc-scripts.html");
  const { selectors: { getSource }, getState } = dbg;
  const sourceUrl = EXAMPLE_URL + "long.js";

  // The source itself doesn't even exist yet, and using
  // `selectSourceURL` will set a pending request to load this source
  // and highlight a specific line.
  dbg.actions.selectSourceURL(sourceUrl, { line: 66 });

  // Wait for the source text to load and make sure we're in the right
  // place.
  yield waitForDispatch(dbg, "LOAD_SOURCE_TEXT");

  // TODO: revisit highlighting lines when the debugger opens
  //assertHighlightLocation(dbg, "long.js", 66);

  // Jump to line 16 and make sure the editor scrolled.
  yield selectSource(dbg, "long.js", 16);
  assertHighlightLocation(dbg, "long.js", 16);

  // Make sure only one line is ever highlighted and the flash
  // animation is cancelled on old lines.
  yield selectSource(dbg, "long.js", 17);
  yield selectSource(dbg, "long.js", 18);
  assertHighlightLocation(dbg, "long.js", 18);
  is(
    findAllElements(dbg, "highlightLine").length,
    1,
    "Only 1 line is highlighted"
  );

  // Test jumping to a line in a source that exists but hasn't been
  // loaded yet.
  selectSource(dbg, "simple1.js", 6);

  // Make sure the source is in the loading state, wait for it to be
  // fully loaded, and check the highlighted line.
  const simple1 = findSource(dbg, "simple1.js");
  ok(getSource(getState(), simple1.id).get("loading"));
  yield waitForDispatch(dbg, "LOAD_SOURCE_TEXT");
  ok(getSource(getState(), simple1.id).get("text"));
  assertHighlightLocation(dbg, "simple1.js", 6);
});
