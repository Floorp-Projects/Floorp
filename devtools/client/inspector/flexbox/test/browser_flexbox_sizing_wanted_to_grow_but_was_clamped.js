/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { getStr } = require("devtools/client/inspector/layout/utils/l10n");

// Test the specific max-clamping scenario where an item wants to grow a certain amount
// but its max-size prevents it from growing that much.

const TEST_URI = URL_ROOT + "doc_flexbox_specific_cases.html";

add_task(async function() {
  await addTab(TEST_URI);
  const { inspector, flexboxInspector } = await openLayoutView();
  const { document: doc } = flexboxInspector;

  info(
    "Select the test item in the document and wait for the sizing info to render"
  );
  const onRendered = waitForDOM(doc, ".flex-outline, .flex-item-sizing", 2);
  await selectNode("#want-to-grow-more-than-max div", inspector);
  const [outlineContainer, sizingContainer] = await onRendered;

  info(
    "Check that the outline contains the max point and that it's equal to final"
  );
  const maxPoint = outlineContainer.querySelector(".flex-outline-point.max");
  ok(maxPoint, "The max point is displayed");
  ok(
    outlineContainer.style.gridTemplateColumns.includes("[final-end max]"),
    "The final and max points are at the same position"
  );

  info("Check that the maximum sizing section displays the right info");
  const reasons = [...sizingContainer.querySelectorAll(".reasons li")];
  const expectedReason = getStr("flexbox.itemSizing.clampedToMax");
  ok(
    reasons.some(r => r.textContent === expectedReason),
    "The clampedToMax reason was found"
  );
});
