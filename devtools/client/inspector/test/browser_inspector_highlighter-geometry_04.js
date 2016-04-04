/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 /* Globals defined in: devtools/client/inspector/test/head.js */

"use strict";

// Test that the arrows and handlers are positioned correctly and have the right
// size.

const TEST_URL = URL_ROOT + "doc_inspector_highlighter-geometry_01.html";
const ID = "geometry-editor-";
const HIGHLIGHTER_TYPE = "GeometryEditorHighlighter";

const handlerMap = {
  "top": {"cx": "x2", "cy": "y2"},
  "bottom": {"cx": "x2", "cy": "y2"},
  "left": {"cx": "x2", "cy": "y2"},
  "right": {"cx": "x2", "cy": "y2"}
};

add_task(function* () {
  let helper = yield openInspectorForURL(TEST_URL)
                       .then(getHighlighterHelperFor(HIGHLIGHTER_TYPE));

  helper.prefix = ID;

  let { hide, finalize } = helper;

  yield checkArrowsAndHandlers(helper, ".absolute-all-4", {
    "top": {x1: 506, y1: 51, x2: 506, y2: 61},
    "bottom": {x1: 506, y1: 451, x2: 506, y2: 251},
    "left": {x1: 401, y1: 156, x2: 411, y2: 156},
    "right": {x1: 901, y1: 156, x2: 601, y2: 156}
  });

  yield checkArrowsAndHandlers(helper, ".relative", {
    "top": {x1: 901, y1: 51, x2: 901, y2: 91},
    "left": {x1: 401, y1: 97, x2: 651, y2: 97}
  });

  yield checkArrowsAndHandlers(helper, ".fixed", {
    "top": {x1: 25, y1: 0, x2: 25, y2: 400},
    "left": {x1: 0, y1: 425, x2: 0, y2: 425}
  });

  info("Hiding the highlighter");
  yield hide();
  yield finalize();
});

function* checkArrowsAndHandlers(helper, selector, arrows) {
  info("Highlighting the test node " + selector);

  yield helper.show(selector);

  for (let side in arrows) {
    yield checkArrowAndHandler(helper, side, arrows[side]);
  }
}

function* checkArrowAndHandler({getElementAttribute}, name, expectedCoords) {
  info("Checking " + name + "arrow and handler coordinates are correct");

  let handlerX = yield getElementAttribute("handler-" + name, "cx");
  let handlerY = yield getElementAttribute("handler-" + name, "cy");

  let expectedHandlerX = yield getElementAttribute("arrow-" + name,
                                handlerMap[name].cx);
  let expectedHandlerY = yield getElementAttribute("arrow-" + name,
                                handlerMap[name].cy);

  is(handlerX, expectedHandlerX,
    "coordinate X for handler " + name + " is correct.");
  is(handlerY, expectedHandlerY,
    "coordinate Y for handler " + name + " is correct.");

  for (let coordinate in expectedCoords) {
    let value = yield getElementAttribute("arrow-" + name, coordinate);

    is(Math.floor(value), expectedCoords[coordinate],
      coordinate + " coordinate for arrow " + name + " is correct");
  }
}
