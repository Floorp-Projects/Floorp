/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test inspector markup view handling focus and blur when moving between markup
// view, its root and other containers, and other parts of inspector.

add_task(function* () {
  let {inspector, testActor} = yield openInspectorForURL(
    "data:text/html;charset=utf-8,<h1>foo</h1><span>bar</span>");
  let markup = inspector.markup;
  let doc = markup.doc;
  let win = doc.defaultView;

  let spanContainer = yield getContainerForSelector("span", inspector);
  let rootContainer = markup.getContainer(markup._rootNode);

  is(doc.activeElement, doc.body,
    "Keyboard focus by default is on document body");

  yield selectNode("span", inspector);

  is(doc.activeElement, doc.body,
    "Keyboard focus is still on document body");

  info("Focusing on the test span node using 'Return' key");
  // Focus on the tree element.
  rootContainer.elt.focus();
  EventUtils.synthesizeKey("VK_RETURN", {}, win);

  is(doc.activeElement, spanContainer.editor.tag,
    "Keyboard focus should be on tag element of focused container");

  info("Focusing on search box, external to markup view document");
  yield focusSearchBoxUsingShortcut(inspector.panelWin);

  is(doc.activeElement, doc.body,
    "Keyboard focus should be removed from focused container");

  info("Selecting the test span node again");
  yield selectNode("span", inspector);

  is(doc.activeElement, doc.body,
    "Keyboard focus should again be on document body");

  info("Focusing on the test span node using 'Space' key");
  // Focus on the tree element.
  rootContainer.elt.focus();
  EventUtils.synthesizeKey("VK_SPACE", {}, win);

  is(doc.activeElement, spanContainer.editor.tag,
    "Keyboard focus should again be on tag element of focused container");

  yield clickOnInspectMenuItem(testActor, "h1");
  is(doc.activeElement, rootContainer.elt,
    "When inspect menu item is used keyboard focus should move to tree.");
});
