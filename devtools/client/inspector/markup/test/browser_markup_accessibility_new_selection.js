/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test inspector markup view handling new node selection that is triggered by
// the user keyboard action. In this case markup tree container must receive
// keyboard focus so that further interactions continue within the markup tree.

add_task(async function() {
  const { inspector } = await openInspectorForURL(
    "data:text/html;charset=utf-8,<h1>foo</h1><span>bar</span>");
  const markup = inspector.markup;
  const doc = markup.doc;
  const rootContainer = markup.getContainer(markup._rootNode);

  is(doc.activeElement, doc.body, "Keyboard focus by default is on document body");

  await selectNode("span", inspector, "test");
  is(doc.activeElement, doc.body, "Keyboard focus remains on document body.");

  await selectNode("h1", inspector, "test-keyboard");
  is(doc.activeElement, rootContainer.elt,
    "Keyboard focus must be on the markup tree conainer.");
});
