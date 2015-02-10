/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test the creation of the SVG highlighter elements of the css transform
// highlighter.

const TEST_URL = "data:text/html;charset=utf-8," +
                 "<div id='transformed' style='border:1px solid red;width:100px;height:100px;transform:skew(13deg);'></div>" +
                 "<div id='untransformed' style='border:1px solid blue;width:100px;height:100px;'></div>" +
                 "<span id='inline' style='transform:rotate(90deg);'>this is an inline transformed element</span>";

add_task(function*() {
  let {inspector, toolbox} = yield openInspectorForURL(TEST_URL);
  let front = inspector.inspector;

  let highlighter = yield front.getHighlighterByType("CssTransformHighlighter");

  yield isHiddenByDefault(highlighter, inspector);
  yield has2PolygonsAnd4Lines(highlighter, inspector);
  yield isNotShownForUntransformed(highlighter, inspector);
  yield isNotShownForInline(highlighter, inspector);
  yield isVisibleWhenShown(highlighter, inspector);
  yield linesLinkThePolygons(highlighter, inspector);

  yield highlighter.finalize();
});

function* isHiddenByDefault(highlighterFront, inspector) {
  info("Checking that the highlighter is hidden by default");

  let hidden = yield getHighlighterNodeAttribute(highlighterFront,
    "css-transform-elements", "hidden");
  ok(hidden, "The highlighter is hidden by default");
}

function* has2PolygonsAnd4Lines(highlighterFront, inspector) {
  info("Checking that the highlighter is made up of 4 lines and 2 polygons");

  let value = yield getHighlighterNodeAttribute(highlighterFront,
    "css-transform-untransformed", "class");
  is(value, "css-transform-untransformed", "The untransformed polygon exists");

  value = yield getHighlighterNodeAttribute(highlighterFront,
    "css-transform-transformed", "class");
  is(value, "css-transform-transformed", "The transformed polygon exists");

  for (let nb of ["1", "2", "3", "4"]) {
    value = yield getHighlighterNodeAttribute(highlighterFront,
      "css-transform-line" + nb, "class");
    is(value, "css-transform-line", "The line " + nb + " exists");
  }
}

function* isNotShownForUntransformed(highlighterFront, inspector) {
  info("Asking to show the highlighter on the untransformed test node");

  let node = yield getNodeFront("#untransformed", inspector);
  yield highlighterFront.show(node);

  let hidden = yield getHighlighterNodeAttribute(highlighterFront,
    "css-transform-elements", "hidden");
  ok(hidden, "The highlighter is still hidden");
}

function* isNotShownForInline(highlighterFront, inspector) {
  info("Asking to show the highlighter on the inline test node");

  let node = yield getNodeFront("#inline", inspector);
  yield highlighterFront.show(node);

  let hidden = yield getHighlighterNodeAttribute(highlighterFront,
    "css-transform-elements", "hidden");
  ok(hidden, "The highlighter is still hidden");
}

function* isVisibleWhenShown(highlighterFront, inspector) {
  info("Asking to show the highlighter on the test node");

  let node = yield getNodeFront("#transformed", inspector);
  yield highlighterFront.show(node);

  let hidden = yield getHighlighterNodeAttribute(highlighterFront,
    "css-transform-elements", "hidden");
  ok(!hidden, "The highlighter is visible");

  info("Hiding the highlighter");
  yield highlighterFront.hide();

  hidden = yield getHighlighterNodeAttribute(highlighterFront,
    "css-transform-elements", "hidden");
  ok(hidden, "The highlighter is hidden");
}

function* linesLinkThePolygons(highlighterFront, inspector) {
  info("Showing the highlighter on the transformed node");

  let node = yield getNodeFront("#transformed", inspector);
  yield highlighterFront.show(node);

  info("Checking that the 4 lines do link the 2 shape's corners");

  let lines = [];
  for (let nb of ["1", "2", "3", "4"]) {
    let x1 = yield getHighlighterNodeAttribute(highlighterFront,
      "css-transform-line" + nb, "x1");
    let y1 = yield getHighlighterNodeAttribute(highlighterFront,
      "css-transform-line" + nb, "y1");
    let x2 = yield getHighlighterNodeAttribute(highlighterFront,
      "css-transform-line" + nb, "x2");
    let y2 = yield getHighlighterNodeAttribute(highlighterFront,
      "css-transform-line" + nb, "y2");
    lines.push({x1, y1, x2, y2});
  }

  let points1 = yield getHighlighterNodeAttribute(highlighterFront,
    "css-transform-untransformed", "points");
  points1 = points1.split(" ");

  let points2 = yield getHighlighterNodeAttribute(highlighterFront,
    "css-transform-transformed", "points");
  points2 = points2.split(" ");

  for (let i = 0; i < lines.length; i++) {
    info("Checking line nb " + i);
    let line = lines[i];

    let p1 = points1[i].split(",");
    is(p1[0], line.x1, "line " + i + "'s first point matches the untransformed x coordinate");
    is(p1[1], line.y1, "line " + i + "'s first point matches the untransformed y coordinate");

    let p2 = points2[i].split(",");
    is(p2[0], line.x2, "line " + i + "'s first point matches the transformed x coordinate");
    is(p2[1], line.y2, "line " + i + "'s first point matches the transformed y coordinate");
  }

  yield highlighterFront.hide();
}
