/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the font-inspector also display fonts when a text node is selected.

const TEST_URI = URL_ROOT + "browser_fontinspector.html";

add_task(async function() {
  let { inspector, view } = await openFontInspectorForURL(TEST_URI);
  let viewDoc = view.document;

  info("Selecting the first text-node first-child of <body>");
  let bodyNode = await getNodeFront("body", inspector);
  let { nodes } = await inspector.walker.children(bodyNode);
  await selectNode(nodes[0], inspector);

  let lis = getUsedFontsEls(viewDoc);
  is(lis.length, 1, "Font inspector shows 1 font");
});
