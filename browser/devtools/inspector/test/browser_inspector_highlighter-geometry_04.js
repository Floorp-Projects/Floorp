/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the arrows are positioned correctly and have the right size.

const TEST_URL = TEST_URL_ROOT + "doc_inspector_highlighter-geometry_01.html";
const ID = "geometry-editor-";

add_task(function*() {
  let {inspector, testActor} = yield openInspectorForURL(TEST_URL);
  let front = inspector.inspector;

  let highlighter = yield front.getHighlighterByType("GeometryEditorHighlighter");

  yield checkArrows(highlighter, inspector, testActor, ".absolute-all-4", {
   "top": {x1: 506, y1: 51, x2: 506, y2: 61},
   "bottom": {x1: 506, y1: 451, x2: 506, y2: 251},
   "left": {x1: 401, y1: 156, x2: 411, y2: 156},
   "right": {x1: 901, y1: 156, x2: 601, y2: 156}
  });

  yield checkArrows(highlighter, inspector, testActor, ".relative", {
   "top": {x1: 901, y1: 51, x2: 901, y2: 91},
   "left": {x1: 401, y1: 97, x2: 651, y2: 97}
  });

  yield checkArrows(highlighter, inspector, testActor, ".fixed", {
   "top": {x1: 25, y1: 0, x2: 25, y2: 400},
   "left": {x1: 0, y1: 425, x2: 0, y2: 425}
  });

  info("Hiding the highlighter");
  yield highlighter.hide();
  yield highlighter.finalize();
});

function* checkArrows(highlighter, inspector, testActor, selector, arrows) {
  info("Highlighting the test node " + selector);
  let node = yield getNodeFront(selector, inspector);
  yield highlighter.show(node);

  for (let side in arrows) {
    yield checkArrow(highlighter, testActor, side, arrows[side]);
  }
}

function* checkArrow(highlighter, testActor, name, expectedCoordinates) {
  for (let coordinate in expectedCoordinates) {
    let value = yield testActor.getHighlighterNodeAttribute(ID + "arrow-" + name, coordinate, highlighter);
    is(Math.floor(value), expectedCoordinates[coordinate],
      coordinate + " coordinate for arrow " + name + " is correct");
  }
}
