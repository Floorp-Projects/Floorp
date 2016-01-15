/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

///////////////////
//
// Whitelisting this test.
// As part of bug 1077403, the leaking uncaught rejection should be fixed.
//
thisTestLeaksUncaughtRejectionsAndShouldBeFixed("false");

// Test that hovering over the markup-view's containers doesn't always show the
// highlighter, depending on the type of node hovered over.

const TEST_PAGE = URL_ROOT +
  "doc_inspector_highlighter-comments.html";

add_task(function* () {
  let {toolbox, inspector, testActor} = yield openInspectorForURL(TEST_PAGE);
  let markupView = inspector.markup;
  yield selectNode("p", inspector);

  info("Hovering over #id1 and waiting for highlighter to appear.");
  yield hoverElement("#id1");
  yield assertHighlighterShownOn(testActor, "#id1");

  info("Hovering over comment node and ensuring highlighter doesn't appear.");
  yield hoverComment();
  yield assertHighlighterHidden(testActor);

  info("Hovering over #id1 again and waiting for highlighter to appear.");
  yield hoverElement("#id1");
  yield assertHighlighterShownOn(testActor, "#id1");

  info("Hovering over #id2 and waiting for highlighter to appear.");
  yield hoverElement("#id2");
  yield assertHighlighterShownOn(testActor, "#id2");

  info("Hovering over <script> and ensuring highlighter doesn't appear.");
  yield hoverElement("script");
  yield assertHighlighterHidden(testActor);

  info("Hovering over #id3 and waiting for highlighter to appear.");
  yield hoverElement("#id3");
  yield assertHighlighterShownOn(testActor, "#id3");

  info("Hovering over hidden #id4 and ensuring highlighter doesn't appear.");
  yield hoverElement("#id4");
  yield assertHighlighterHidden(testActor);

  function hoverContainer(container) {
    let promise = inspector.toolbox.once("node-highlight");
    EventUtils.synthesizeMouse(container.tagLine, 2, 2, {type: "mousemove"},
        markupView.doc.defaultView);

    return promise;
  }

  function* hoverElement(selector) {
    info("Hovering node " + selector + " in the markup view");
    let container = yield getContainerForSelector(selector, inspector);
    return hoverContainer(container);
  }

  function hoverComment() {
    info("Hovering the comment node in the markup view");
    for (let [node, container] of markupView._containers) {
      if (node.nodeType === Ci.nsIDOMNode.COMMENT_NODE) {
        return hoverContainer(container);
      }
    }
  }

  function* assertHighlighterShownOn(testActor, selector) {
    ok((yield testActor.assertHighlightedNode(selector)), "Highlighter is shown on the right node: " + selector);
  }

  function* assertHighlighterHidden(testActor) {
    let isVisible = yield testActor.isHighlighting();
    ok(!isVisible, "Highlighter is hidden");
  }
});
