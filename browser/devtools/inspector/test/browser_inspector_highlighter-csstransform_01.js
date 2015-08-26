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
  let {inspector, toolbox, testActor} = yield openInspectorForURL(TEST_URL);
  let front = inspector.inspector;

  let highlighter = yield front.getHighlighterByType("CssTransformHighlighter");

  yield isHiddenByDefault(testActor, highlighter);
  yield has2PolygonsAnd4Lines(testActor, highlighter);
  yield isNotShownForUntransformed(testActor, inspector, highlighter);
  yield isNotShownForInline(testActor, inspector, highlighter);
  yield isVisibleWhenShown(testActor, inspector, highlighter);
  yield linesLinkThePolygons(testActor, inspector, highlighter);

  yield highlighter.finalize();
});

function* isHiddenByDefault(testActor, highlighterFront) {
  info("Checking that the highlighter is hidden by default");

  let hidden = yield testActor.getHighlighterNodeAttribute("css-transform-elements", "hidden", highlighterFront);
  ok(hidden, "The highlighter is hidden by default");
}

function* has2PolygonsAnd4Lines(testActor, highlighterFront) {
  info("Checking that the highlighter is made up of 4 lines and 2 polygons");

  let value = yield testActor.getHighlighterNodeAttribute("css-transform-untransformed", "class", highlighterFront);
  is(value, "css-transform-untransformed", "The untransformed polygon exists");

  value = yield testActor.getHighlighterNodeAttribute("css-transform-transformed", "class", highlighterFront);
  is(value, "css-transform-transformed", "The transformed polygon exists");

  for (let nb of ["1", "2", "3", "4"]) {
    value = yield testActor.getHighlighterNodeAttribute("css-transform-line" + nb, "class", highlighterFront);
    is(value, "css-transform-line", "The line " + nb + " exists");
  }
}

function* isNotShownForUntransformed(testActor, inspector, highlighterFront) {
  info("Asking to show the highlighter on the untransformed test node");

  let node = yield getNodeFront("#untransformed", inspector);
  yield highlighterFront.show(node);

  let hidden = yield testActor.getHighlighterNodeAttribute("css-transform-elements", "hidden", highlighterFront);
  ok(hidden, "The highlighter is still hidden");
}

function* isNotShownForInline(testActor, inspector, highlighterFront) {
  info("Asking to show the highlighter on the inline test node");

  let node = yield getNodeFront("#inline", inspector);
  yield highlighterFront.show(node);

  let hidden = yield testActor.getHighlighterNodeAttribute("css-transform-elements", "hidden", highlighterFront);
  ok(hidden, "The highlighter is still hidden");
}

function* isVisibleWhenShown(testActor, inspector, highlighterFront) {
  info("Asking to show the highlighter on the test node");

  let node = yield getNodeFront("#transformed", inspector);
  yield highlighterFront.show(node);

  let hidden = yield testActor.getHighlighterNodeAttribute("css-transform-elements", "hidden", highlighterFront);
  ok(!hidden, "The highlighter is visible");

  info("Hiding the highlighter");
  yield highlighterFront.hide();

  hidden = yield testActor.getHighlighterNodeAttribute("css-transform-elements", "hidden", highlighterFront);
  ok(hidden, "The highlighter is hidden");
}

function* linesLinkThePolygons(testActor, inspector, highlighterFront) {
  info("Showing the highlighter on the transformed node");

  let node = yield getNodeFront("#transformed", inspector);
  yield highlighterFront.show(node);

  info("Checking that the 4 lines do link the 2 shape's corners");

  let lines = [];
  for (let nb of ["1", "2", "3", "4"]) {
    let x1 = yield testActor.getHighlighterNodeAttribute("css-transform-line" + nb, "x1", highlighterFront);
    let y1 = yield testActor.getHighlighterNodeAttribute("css-transform-line" + nb, "y1", highlighterFront);
    let x2 = yield testActor.getHighlighterNodeAttribute("css-transform-line" + nb, "x2", highlighterFront);
    let y2 = yield testActor.getHighlighterNodeAttribute("css-transform-line" + nb, "y2", highlighterFront);
    lines.push({x1, y1, x2, y2});
  }

  let points1 = yield testActor.getHighlighterNodeAttribute("css-transform-untransformed", "points", highlighterFront);
  points1 = points1.split(" ");

  let points2 = yield testActor.getHighlighterNodeAttribute("css-transform-transformed", "points", highlighterFront);
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
