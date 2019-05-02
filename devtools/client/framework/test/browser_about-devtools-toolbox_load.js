/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that about:devtools-toolbox shows error an page when opened with invalid
 * paramters
 */
add_task(async function() {
  // test that error is shown when missing `type` param
  let { document, tab } = await openAboutToolbox({ invalid: "invalid" });
  await assertErrorIsShown(document);
  await removeTab(tab);
  // test that error is shown if `id` is not provided
  ({ document, tab } = await openAboutToolbox({ type: "tab" }));
  await assertErrorIsShown(document);
  await removeTab(tab);
  // test that error is shown if `remoteId` refers to an unexisting target
  ({ document, tab } = await openAboutToolbox({ type: "tab", remoteId: "13371337" }));
  await assertErrorIsShown(document);
  await removeTab(tab);

  async function assertErrorIsShown(doc) {
    await waitUntil(() => doc.querySelector(".qa-error-page"));
    ok(doc.querySelector(".qa-error-page"), "Error page is rendered");
  }
});
