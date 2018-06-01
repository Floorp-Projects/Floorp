/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test element node name in the markupview
const TEST_URL = URL_ROOT + "doc_markup_html_mixed_case.html";

add_task(async function() {
  const {inspector} = await openInspectorForURL(TEST_URL);

  // Get and open the svg element to show its children
  const svgNodeFront = await getNodeFront("svg", inspector);
  await inspector.markup.expandNode(svgNodeFront);
  await waitForMultipleChildrenUpdates(inspector);

  const clipPathContainer = await getContainerForSelector("clipPath", inspector);
  info("Checking the clipPath element");
  ok(clipPathContainer.editor.tag.textContent === "clipPath",
     "clipPath node name is not lowercased");

  const divContainer = await getContainerForSelector("div", inspector);

  info("Checking the div element");
  ok(divContainer.editor.tag.textContent === "div",
     "div node name is lowercased");
});
