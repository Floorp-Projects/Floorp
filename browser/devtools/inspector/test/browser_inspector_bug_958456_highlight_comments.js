/* -*- Mode: Javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that hovering over the markup-view's containers doesn't always show the
// highlighter, depending on the type of node hovered over.

let {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
let promise = devtools.require("sdk/core/promise");
let {Task} = Cu.import("resource://gre/modules/Task.jsm", {});

const TEST_PAGE = "http://mochi.test:8888/browser/browser/devtools/inspector/test/browser_inspector_bug_958456_highlight_comments.html";
let inspector, markupView, doc;

function test() {
  waitForExplicitFinish();
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    doc = content.document;

    waitForFocus(function() {
      openInspector((aInspector, aToolbox) => {
        inspector = aInspector;
        markupView = inspector.markup;
        inspector.once("inspector-updated", startTests);
      });
    }, content);
  }, true);
  content.location = TEST_PAGE;
}

function startTests() {
  Task.spawn(function() {
    yield prepareHighlighter();

    // Highlighter should be shown on this element
    yield hoverElement("#id1");
    assertHighlighterShownOn("#id1");

    // But not over a comment
    yield hoverComment();
    assertHighlighterHidden();

    // It should be shown again when coming back to the same element tough
    yield hoverElement("#id1");
    assertHighlighterShownOn("#id1");

    // Shown here too
    yield hoverElement("#id2");
    assertHighlighterShownOn("#id2");

    // But not over a script tag
    yield hoverElement("script");
    assertHighlighterHidden();

    // Shown here
    yield hoverElement("#id3");
    assertHighlighterShownOn("#id3");

    // But not over a hidden element
    yield hoverElement("#id4");
    assertHighlighterHidden();
  }).then(null, Cu.reportError).then(finishTest);
}

function finishTest() {
  doc = inspector = markupView = null;
  gBrowser.removeCurrentTab();
  finish();
}

function prepareHighlighter() {
  // Make sure the highlighter doesn't have transitions enabled
  let deferred = promise.defer();
  inspector.selection.setNode(doc.querySelector("p"), null);
  inspector.once("inspector-updated", () => {
    getHighlighterOutline().setAttribute("disable-transitions", "true");
    deferred.resolve();
  });
  return deferred.promise;
}

function hoverContainer(container) {
  let deferred = promise.defer();
  EventUtils.synthesizeMouse(container.tagLine, 2, 2, {type: "mousemove"},
      markupView.doc.defaultView);
  inspector.toolbox.once("node-highlight", deferred.resolve);
  return deferred.promise;
}

function hoverElement(selector) {
  info("Hovering node " + selector + " in the markup view");
  let container = getContainerForRawNode(markupView, doc.querySelector(selector));
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
  let node = doc.querySelector(selector);
  let highlightNode = getHighlitNode();
  is(node, highlightNode, "Highlighter is shown on the right node: " + selector);
}

function assertHighlighterHidden() {
  ok(!isHighlighting(), "Highlighter is hidden");
}
