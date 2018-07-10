/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that selecting a text node invokes the font editor on its parent node.

const TEST_URI = URL_ROOT + "browser_fontinspector.html";

add_task(async function() {
  await pushPref("devtools.inspector.fonteditor.enabled", true);
  const { inspector, view } = await openFontInspectorForURL(TEST_URI);
  const viewDoc = view.document;

  info("Select the first text node of <body>");
  const bodyNode = await getNodeFront("body", inspector);
  const { nodes } = await inspector.walker.children(bodyNode);
  const onInspectorUpdated = inspector.once("fontinspector-updated");
  await selectNode(nodes[0], inspector);

  info("Waiting for font editor to render");
  await onInspectorUpdated;

  const textFonts = getUsedFontsEls(viewDoc);

  info("Select the <body> element");
  await selectNode("body", inspector);

  const parentFonts = getUsedFontsEls(viewDoc);
  is(textFonts.length, parentFonts.length, "Font inspector shows same number of fonts");
});
