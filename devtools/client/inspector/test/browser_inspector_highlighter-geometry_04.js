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

add_task(async function() {
  const helper = await openInspectorForURL(TEST_URL)
                       .then(getHighlighterHelperFor(HIGHLIGHTER_TYPE));

  helper.prefix = ID;

  const { hide, finalize } = helper;

  await checkArrowsAndHandlers(helper, ".absolute-all-4", {
    "top": {x1: 506, y1: 51, x2: 506, y2: 61},
    "bottom": {x1: 506, y1: 451, x2: 506, y2: 251},
    "left": {x1: 401, y1: 156, x2: 411, y2: 156},
    "right": {x1: 901, y1: 156, x2: 601, y2: 156}
  });

  await checkArrowsAndHandlers(helper, ".relative", {
    "top": {x1: 901, y1: 51, x2: 901, y2: 91},
    "left": {x1: 401, y1: 97, x2: 651, y2: 97}
  });

  await checkArrowsAndHandlers(helper, ".fixed", {
    "top": {x1: 25, y1: 0, x2: 25, y2: 400},
    "left": {x1: 0, y1: 425, x2: 0, y2: 425}
  });

  info("Hiding the highlighter");
  await hide();
  await finalize();
});

async function checkArrowsAndHandlers(helper, selector, arrows) {
  info("Highlighting the test node " + selector);

  await helper.show(selector);

  for (const side in arrows) {
    await checkArrowAndHandler(helper, side, arrows[side]);
  }
}

async function checkArrowAndHandler({getElementAttribute}, name, expectedCoords) {
  info("Checking " + name + "arrow and handler coordinates are correct");

  const handlerX = await getElementAttribute("handler-" + name, "cx");
  const handlerY = await getElementAttribute("handler-" + name, "cy");

  const expectedHandlerX = await getElementAttribute("arrow-" + name,
                                handlerMap[name].cx);
  const expectedHandlerY = await getElementAttribute("arrow-" + name,
                                handlerMap[name].cy);

  is(handlerX, expectedHandlerX,
    "coordinate X for handler " + name + " is correct.");
  is(handlerY, expectedHandlerY,
    "coordinate Y for handler " + name + " is correct.");

  for (const coordinate in expectedCoords) {
    const value = await getElementAttribute("arrow-" + name, coordinate);

    is(Math.floor(value), expectedCoords[coordinate],
      coordinate + " coordinate for arrow " + name + " is correct");
  }
}
