/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the browser remembers the previous scroll position after reload, even with
// Inspector opened - Bug 1382341.
// NOTE: Using a real file instead data: URL since the bug won't happen on data: URL
const TEST_URI = URL_ROOT + "doc_inspector_highlighter_scroll.html";

add_task(async function() {
  const { inspector } = await openInspectorForURL(TEST_URI);

  await scrollContentPageNodeIntoView(gBrowser.selectedBrowser, "a");
  await selectAndHighlightNode("a", inspector);

  const markupLoaded = inspector.once("markuploaded");

  const y = await getPageYOffset();
  isnot(y, 0, "window scrolled vertically.");

  info("Reloading page.");
  await reloadBrowser();

  info("Waiting for markupview to load after reload.");
  await markupLoaded;

  const newY = await getPageYOffset();
  is(y, newY, "window remember the previous scroll position.");
});

async function getPageYOffset() {
  return SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    () => content.pageYOffset
  );
}
