/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Check that custom highlighters can be retrieved by type and that they expose
// the expected API.

const TEST_URL = "data:text/html;charset=utf-8,custom highlighters";

add_task(function* () {
  let {inspector, toolbox} = yield openInspectorForURL(TEST_URL);

  yield onlyOneInstanceOfMainHighlighter(inspector);
  yield manyInstancesOfCustomHighlighters(inspector);
  yield showHideMethodsAreAvailable(inspector);
  yield unknownHighlighterTypeShouldntBeAccepted(inspector);
});

function* onlyOneInstanceOfMainHighlighter({inspector}) {
  info("Check that the inspector always sends back the same main highlighter");

  let h1 = yield inspector.getHighlighter(false);
  let h2 = yield inspector.getHighlighter(false);
  is(h1, h2, "The same highlighter front was returned");

  is(h1.typeName, "highlighter", "The right front type was returned");
}

function* manyInstancesOfCustomHighlighters({inspector}) {
  let h1 = yield inspector.getHighlighterByType("BoxModelHighlighter");
  let h2 = yield inspector.getHighlighterByType("BoxModelHighlighter");
  ok(h1 !== h2, "getHighlighterByType returns new instances every time (1)");

  let h3 = yield inspector.getHighlighterByType("CssTransformHighlighter");
  let h4 = yield inspector.getHighlighterByType("CssTransformHighlighter");
  ok(h3 !== h4, "getHighlighterByType returns new instances every time (2)");
  ok(h3 !== h1 && h3 !== h2,
    "getHighlighterByType returns new instances every time (3)");
  ok(h4 !== h1 && h4 !== h2,
    "getHighlighterByType returns new instances every time (4)");

  yield h1.finalize();
  yield h2.finalize();
  yield h3.finalize();
  yield h4.finalize();
}

function* showHideMethodsAreAvailable({inspector}) {
  let h1 = yield inspector.getHighlighterByType("BoxModelHighlighter");
  let h2 = yield inspector.getHighlighterByType("CssTransformHighlighter");

  ok("show" in h1, "Show method is present on the front API");
  ok("show" in h2, "Show method is present on the front API");
  ok("hide" in h1, "Hide method is present on the front API");
  ok("hide" in h2, "Hide method is present on the front API");

  yield h1.finalize();
  yield h2.finalize();
}

function* unknownHighlighterTypeShouldntBeAccepted({inspector}) {
  let h = yield inspector.getHighlighterByType("whatever");
  ok(!h, "No highlighter was returned for the invalid type");
}
