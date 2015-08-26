/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the custom rect highlighter provides the right API, ensures that
// the input is valid and that it does create a box with the right dimensions,
// at the right position.

const TEST_URL = "data:text/html;charset=utf-8,Rect Highlighter Test";

add_task(function*() {
  let {inspector, toolbox, testActor} = yield openInspectorForURL(TEST_URL);
  let front = inspector.inspector;
  let highlighter = yield front.getHighlighterByType("RectHighlighter");
  let body = yield getNodeFront("body", inspector);

  info("Make sure the highlighter returned is correct");

  ok(highlighter, "The RectHighlighter custom type was created");
  is(highlighter.typeName, "customhighlighter",
    "The RectHighlighter has the right type");
  ok(highlighter.show && highlighter.hide,
    "The RectHighlighter has the expected show/hide methods");

  info("Check that the highlighter is hidden by default");

  let hidden = yield testActor.getHighlighterNodeAttribute("highlighted-rect", "hidden", highlighter);
  is(hidden, "true", "The highlighter is hidden by default");

  info("Check that nothing is shown if no rect is passed");

  yield highlighter.show(body);
  hidden = yield testActor.getHighlighterNodeAttribute("highlighted-rect", "hidden", highlighter);
  is(hidden, "true", "The highlighter is hidden when no rect is passed");

  info("Check that nothing is shown if rect is incomplete or invalid");

  yield highlighter.show(body, {
    rect: {x: 0, y: 0}
  });
  hidden = yield testActor.getHighlighterNodeAttribute("highlighted-rect", "hidden", highlighter);
  is(hidden, "true", "The highlighter is hidden when the rect is incomplete");

  yield highlighter.show(body, {
    rect: {x: 0, y: 0, width: -Infinity, height: 0}
  });
  hidden = yield testActor.getHighlighterNodeAttribute("highlighted-rect", "hidden", highlighter);
  is(hidden, "true", "The highlighter is hidden when the rect is invalid (1)");

  yield highlighter.show(body, {
    rect: {x: 0, y: 0, width: 5, height: -45}
  });
  hidden = yield testActor.getHighlighterNodeAttribute("highlighted-rect", "hidden", highlighter);
  is(hidden, "true", "The highlighter is hidden when the rect is invalid (2)");

  yield highlighter.show(body, {
    rect: {x: "test", y: 0, width: 5, height: 5}
  });
  hidden = yield testActor.getHighlighterNodeAttribute("highlighted-rect", "hidden", highlighter);
  is(hidden, "true", "The highlighter is hidden when the rect is invalid (3)");

  info("Check that the highlighter is displayed when valid options are passed");

  yield highlighter.show(body, {
    rect: {x: 5, y: 5, width: 50, height: 50}
  });
  hidden = yield testActor.getHighlighterNodeAttribute("highlighted-rect", "hidden", highlighter);
  ok(!hidden, "The highlighter is displayed");
  let style = yield testActor.getHighlighterNodeAttribute("highlighted-rect", "style", highlighter);
  is(style, "left:5px;top:5px;width:50px;height:50px;",
    "The highlighter is positioned correctly");

  info("Check that the highlighter can be displayed at x=0 y=0");

  yield highlighter.show(body, {
    rect: {x: 0, y: 0, width: 50, height: 50}
  });
  hidden = yield testActor.getHighlighterNodeAttribute("highlighted-rect", "hidden", highlighter);
  ok(!hidden, "The highlighter is displayed when x=0 and y=0");
  style = yield testActor.getHighlighterNodeAttribute("highlighted-rect", "style", highlighter);
  is(style, "left:0px;top:0px;width:50px;height:50px;",
    "The highlighter is positioned correctly");

  info("Check that the highlighter is hidden when dimensions are 0");

  yield highlighter.show(body, {
    rect: {x: 0, y: 0, width: 0, height: 0}
  });
  hidden = yield testActor.getHighlighterNodeAttribute("highlighted-rect", "hidden", highlighter);
  is(hidden, "true", "The highlighter is hidden width and height are 0");

  info("Check that a fill color can be passed");

  yield highlighter.show(body, {
    rect: {x: 100, y: 200, width: 500, height: 200},
    fill: "red"
  });
  hidden = yield testActor.getHighlighterNodeAttribute("highlighted-rect", "hidden", highlighter);
  ok(!hidden, "The highlighter is displayed");
  style = yield testActor.getHighlighterNodeAttribute("highlighted-rect", "style", highlighter);
  is(style, "left:100px;top:200px;width:500px;height:200px;background:red;",
    "The highlighter has the right background color");

  yield highlighter.hide();
  yield highlighter.finalize();
});
