/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const VALID_JSON_URL = URL_ROOT + "valid_json.json";

add_task(async function () {
  info("Test focus JSON view started");
  await addJsonViewTab(VALID_JSON_URL);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async () => {
    const scroller = content.document.getElementById("json-scrolling-panel");
    ok(scroller, "Found the scrollable area");
    is(
      content.document.activeElement,
      scroller,
      "Scrollable area initially focused"
    );
  });
  await reloadBrowser();
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async () => {
    const scroller = await ContentTaskUtils.waitForCondition(
      () => content.document.getElementById("json-scrolling-panel"),
      "Wait for the panel to be loaded"
    );
    is(
      content.document.activeElement,
      scroller,
      "Scrollable area focused after reload"
    );
  });
});
