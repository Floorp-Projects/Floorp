/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { getStr } = require("devtools/client/inspector/layout/utils/l10n");

// Non-regression for bug 1501263.
// In this bug, an item that was set to grow, with a max-width, and growing siblings
// was marked as "couldn't grow because siblings have used the extra space". This was
// wrong because the item was only being clamped by its max-width.
// This test prevents this from happening again.

const TEST_URI = URL_ROOT + "doc_flexbox_specific_cases.html";

add_task(async function() {
  await addTab(TEST_URI);
  const { inspector, flexboxInspector } = await openLayoutView();
  const { document: doc } = flexboxInspector;

  info("Select the test item in the document and wait for the sizing info to render");
  const onFlexibilityReasonsRendered = waitForDOM(doc,
    "ul.flex-item-sizing .section.flexibility .reasons");
  await selectNode("#grow-not-grow div:first-child", inspector);
  const [reasonsList] = await onFlexibilityReasonsRendered;

  info("Get the list of reasons in the flexibility section");
  const [...reasons] = reasonsList.querySelectorAll("li");
  const str = getStr("flexbox.itemSizing.growthAttemptButSiblings");
  ok(reasons.every(r => r.textContent !== str),
     "The 'could not grow because of siblings' reason was not found");
});
