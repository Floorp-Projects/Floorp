/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test inspector markup view handling focus and blur when moving between markup
// view, its root and other containers, and other parts of inspector.

add_task(async function() {
  const {inspector, testActor} = await openInspectorForURL(
    "data:text/html;charset=utf-8,<h1>foo</h1><span>bar</span>");
  const markup = inspector.markup;
  const doc = markup.doc;
  const win = doc.defaultView;

  const spanContainer = await getContainerForSelector("span", inspector);
  const rootContainer = markup.getContainer(markup._rootNode);

  is(doc.activeElement, doc.body,
    "Keyboard focus by default is on document body");

  await selectNode("span", inspector);

  is(doc.activeElement, doc.body,
    "Keyboard focus is still on document body");

  info("Focusing on the test span node using 'Return' key");
  // Focus on the tree element.
  rootContainer.elt.focus();
  EventUtils.synthesizeKey("VK_RETURN", {}, win);

  is(doc.activeElement, spanContainer.editor.tag,
    "Keyboard focus should be on tag element of focused container");

  info("Focusing on search box, external to markup view document");
  await focusSearchBoxUsingShortcut(inspector.panelWin);

  is(doc.activeElement, doc.body,
    "Keyboard focus should be removed from focused container");

  info("Selecting the test span node again");
  await selectNode("span", inspector);

  is(doc.activeElement, doc.body,
    "Keyboard focus should again be on document body");

  info("Focusing on the test span node using 'Space' key");
  // Focus on the tree element.
  rootContainer.elt.focus();
  EventUtils.synthesizeKey("VK_SPACE", {}, win);

  is(doc.activeElement, spanContainer.editor.tag,
    "Keyboard focus should again be on tag element of focused container");

  await clickOnInspectMenuItem(testActor, "h1");
  is(doc.activeElement, rootContainer.elt,
    "When inspect menu item is used keyboard focus should move to tree.");
});
