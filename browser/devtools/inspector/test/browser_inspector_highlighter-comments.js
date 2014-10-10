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

const TEST_PAGE = TEST_URL_ROOT +
  "doc_inspector_highlighter-comments.html";

let test = asyncTest(function* () {
  let { inspector } = yield openInspectorForURL(TEST_PAGE);
  let markupView = inspector.markup;
  yield selectNode("p", inspector);

  info("Hovering over #id1 and waiting for highlighter to appear.");
  yield hoverElement("#id1");
  assertHighlighterShownOn("#id1");

  info("Hovering over comment node and ensuring highlighter doesn't appear.");
  yield hoverComment();
  assertHighlighterHidden();

  info("Hovering over #id1 again and waiting for highlighter to appear.");
  yield hoverElement("#id1");
  assertHighlighterShownOn("#id1");

  info("Hovering over #id2 and waiting for highlighter to appear.");
  yield hoverElement("#id2");
  assertHighlighterShownOn("#id2");

  info("Hovering over <script> and ensuring highlighter doesn't appear.");
  yield hoverElement("script");
  assertHighlighterHidden();

  info("Hovering over #id3 and waiting for highlighter to appear.");
  yield hoverElement("#id3");
  assertHighlighterShownOn("#id3");

  info("Hovering over hidden #id4 and ensuring highlighter doesn't appear.");
  yield hoverElement("#id4");
  assertHighlighterHidden();

  function hoverContainer(container) {
    let promise = inspector.toolbox.once("node-highlight");
    EventUtils.synthesizeMouse(container.tagLine, 2, 2, {type: "mousemove"},
        markupView.doc.defaultView);

    return promise;
  }

  function hoverElement(selector) {
    info("Hovering node " + selector + " in the markup view");
    let container = getContainerForRawNode(markupView, getNode(selector));
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

  function assertHighlighterShownOn(selector) {
    let node = getNode(selector);
    let highlightNode = getHighlitNode();
    is(node, highlightNode, "Highlighter is shown on the right node: " + selector);
  }

  function assertHighlighterHidden() {
    ok(!isHighlighting(), "Highlighter is hidden");
  }
});
