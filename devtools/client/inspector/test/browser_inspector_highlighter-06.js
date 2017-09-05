/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the browser remembers the previous scroll position after reload, even with
// Inspector opened - Bug 1382341.
// NOTE: Using a real file instead data: URL since the bug won't happen on data: URL
const TEST_URI = URL_ROOT + "doc_inspector_highlighter_scroll.html";

add_task(function* () {
  let { inspector, testActor } = yield openInspectorForURL(TEST_URI);

  yield testActor.scrollIntoView("a");
  yield selectAndHighlightNode("a", inspector);

  let markupLoaded = inspector.once("markuploaded");

  let y = yield testActor.eval("window.pageYOffset");
  isnot(y, 0, "window scrolled vertically.");

  info("Reloading page.");
  yield testActor.reload();

  info("Waiting for markupview to load after reload.");
  yield markupLoaded;

  let newY = yield testActor.eval("window.pageYOffset");
  is(y, newY, "window remember the previous scroll position.");
});
