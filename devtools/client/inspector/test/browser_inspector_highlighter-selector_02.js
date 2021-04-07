/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the custom selector highlighter creates highlighters for nodes in
// the right frame.

const FRAME_SRC =
  "data:text/html;charset=utf-8," + "<div class=sub-level-node></div>";

const TEST_URL =
  "data:text/html;charset=utf-8," +
  "<div class=root-level-node></div>" +
  '<iframe src="' +
  FRAME_SRC +
  '" />';

const TEST_DATA = [
  {
    selector: ".root-level-node",
    containerCount: 1,
  },
  {
    selector: ".sub-level-node",
    containerCount: 0,
  },
  {
    inIframe: true,
    selector: ".root-level-node",
    containerCount: 0,
  },
  {
    inIframe: true,
    selector: ".sub-level-node",
    containerCount: 1,
  },
];

requestLongerTimeout(5);

add_task(async function() {
  const { inspector, testActor } = await openInspectorForURL(TEST_URL);
  const front = inspector.inspectorFront;
  const highlighter = await front.getHighlighterByType("SelectorHighlighter");

  for (const { inIframe, selector, containerCount } of TEST_DATA) {
    info(
      "Showing the highlighter on " +
        selector +
        ". Expecting " +
        containerCount +
        " highlighter containers"
    );

    let contextNode;
    if (inIframe) {
      contextNode = await getNodeFrontInFrames(["iframe", "body"], inspector);
    } else {
      contextNode = await getNodeFront("body", inspector);
    }

    await highlighter.show(contextNode, { selector });

    const nb = await testActor.getSelectorHighlighterBoxNb(highlighter.actorID);
    ok(nb !== null, "The number of highlighters was retrieved");

    is(nb, containerCount, "The correct number of highlighers were created");
    await highlighter.hide();
  }

  await highlighter.finalize();
});
