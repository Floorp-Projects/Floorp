/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the custom rect highlighter positions the rectangle relative to the
// viewport of the context node we pass to it.

const TEST_URL = URL_ROOT + "doc_inspector_highlighter_rect.html";

add_task(function* () {
  let {inspector, testActor} = yield openInspectorForURL(TEST_URL);
  let front = inspector.inspector;
  let highlighter = yield front.getHighlighterByType("RectHighlighter");

  info("Showing the rect highlighter in the context of the iframe");

  // Get the reference to a context node inside the iframe
  let childBody = yield getNodeFrontInFrame("body", "iframe", inspector);
  yield highlighter.show(childBody, {
    rect: {x: 50, y: 50, width: 100, height: 100}
  });

  let style = yield testActor.getHighlighterNodeAttribute("highlighted-rect", "style", highlighter);

  // The parent body has margin=50px and border=10px
  // The parent iframe also has margin=50px and border=10px
  // = 50 + 10 + 50 + 10 = 120px
  // The rect is aat x=50 and y=50, so left and top should be 170px
  is(style, "left:170px;top:170px;width:100px;height:100px;",
    "The highlighter is correctly positioned");

  yield highlighter.hide();
  yield highlighter.finalize();
});
