/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test namespaced element node names in the markupview.

const XHTML = `
  <!DOCTYPE html>
  <html xmlns="http://www.w3.org/1999/xhtml"
        xmlns:svg="http://www.w3.org/2000/svg">
    <body>
      <svg:svg width="100" height="100">
        <svg:clipPath id="clip">
          <svg:rect id="rectangle" x="0" y="0" width="10" height="5"></svg:rect>
        </svg:clipPath>
        <svg:circle cx="0" cy="0" r="5"></svg:circle>
      </svg:svg>
    </body>
  </html>
`;

const TEST_URI = "data:application/xhtml+xml;charset=utf-8," + encodeURI(XHTML);

add_task(function* () {
  let {inspector} = yield openInspectorForURL(TEST_URI);

  // Get and open the svg element to show its children.
  let svgNodeFront = yield getNodeFront("svg", inspector);
  yield inspector.markup.expandNode(svgNodeFront);
  yield waitForMultipleChildrenUpdates(inspector);

  let clipPathContainer = yield getContainerForSelector("clipPath", inspector);
  info("Checking the clipPath element");
  ok(clipPathContainer.editor.tag.textContent === "svg:clipPath",
     "svg:clipPath node is correctly displayed");

  let circlePathContainer = yield getContainerForSelector("circle", inspector);
  info("Checking the circle element");
  ok(circlePathContainer.editor.tag.textContent === "svg:circle",
     "svg:circle node is correctly displayed");
});
