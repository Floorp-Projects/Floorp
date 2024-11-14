/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const URL = `data:image/svg+xml,<svg viewBox="0 0 100 100" xmlns="http://www.w3.org/2000/svg"><rect width="100" height="100" fill="green"/></svg>`;

function test_once() {
  return BrowserTestUtils.withNewTab(URL, async browser => {
    return await SpecialPowers.spawn(browser, [], async function () {
      const rect = content.document.documentElement.getBoundingClientRect();
      info(
        `${rect.width}x${rect.height}, ${content.innerWidth}x${content.innerHeight}`
      );
      is(
        Math.round(rect.height),
        content.innerHeight,
        "Should fill the viewport and not overflow"
      );
    });
  });
}

add_task(async function test_with_no_text_zoom() {
  await test_once();
});

add_task(async function test_with_text_zoom() {
  let dpi = window.devicePixelRatio;

  await SpecialPowers.pushPrefEnv({ set: [["ui.textScaleFactor", 200]] });
  Assert.greater(
    window.devicePixelRatio,
    dpi,
    "DPI should change as a result of the pref flip"
  );

  return test_once();
});
