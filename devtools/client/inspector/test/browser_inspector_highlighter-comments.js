/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Test that hovering over the markup-view's containers doesn't always show the
// highlighter, depending on the type of node hovered over.

const TEST_PAGE = URL_ROOT +
  "doc_inspector_highlighter-comments.html";

add_task(async function() {
  let {inspector, testActor} = await openInspectorForURL(TEST_PAGE);
  let markupView = inspector.markup;
  await selectNode("p", inspector);

  info("Hovering over #id1 and waiting for highlighter to appear.");
  await hoverElement("#id1");
  await assertHighlighterShownOn("#id1");

  info("Hovering over comment node and ensuring highlighter doesn't appear.");
  await hoverComment();
  await assertHighlighterHidden();

  info("Hovering over #id1 again and waiting for highlighter to appear.");
  await hoverElement("#id1");
  await assertHighlighterShownOn("#id1");

  info("Hovering over #id2 and waiting for highlighter to appear.");
  await hoverElement("#id2");
  await assertHighlighterShownOn("#id2");

  info("Hovering over <script> and ensuring highlighter doesn't appear.");
  await hoverElement("script");
  await assertHighlighterHidden();

  info("Hovering over #id3 and waiting for highlighter to appear.");
  await hoverElement("#id3");
  await assertHighlighterShownOn("#id3");

  info("Hovering over hidden #id4 and ensuring highlighter doesn't appear.");
  await hoverElement("#id4");
  await assertHighlighterHidden();

  info("Hovering over a text node and waiting for highlighter to appear.");
  await hoverTextNode("Visible text node");
  await assertHighlighterShownOnTextNode("body", 14);

  function hoverContainer(container) {
    let promise = inspector.toolbox.once("node-highlight");

    container.tagLine.scrollIntoView();
    EventUtils.synthesizeMouse(container.tagLine, 2, 2, {type: "mousemove"},
        markupView.doc.defaultView);

    return promise;
  }

  async function hoverElement(selector) {
    info(`Hovering node ${selector} in the markup view`);
    let container = await getContainerForSelector(selector, inspector);
    return hoverContainer(container);
  }

  function hoverComment() {
    info("Hovering the comment node in the markup view");
    for (let [node, container] of markupView._containers) {
      if (node.nodeType === Node.COMMENT_NODE) {
        return hoverContainer(container);
      }
    }
    return null;
  }

  function hoverTextNode(text) {
    info(`Hovering the text node "${text}" in the markup view`);
    let container = [...markupView._containers].filter(([nodeFront]) => {
      return nodeFront.nodeType === Node.TEXT_NODE &&
             nodeFront._form.nodeValue.trim() === text.trim();
    })[0][1];
    return hoverContainer(container);
  }

  async function assertHighlighterShownOn(selector) {
    ok((await testActor.assertHighlightedNode(selector)),
       "Highlighter is shown on the right node: " + selector);
  }

  async function assertHighlighterShownOnTextNode(parentSelector, childNodeIndex) {
    ok((await testActor.assertHighlightedTextNode(parentSelector, childNodeIndex)),
       "Highlighter is shown on the right text node");
  }

  async function assertHighlighterHidden() {
    let isVisible = await testActor.isHighlighting();
    ok(!isVisible, "Highlighter is hidden");
  }
});
