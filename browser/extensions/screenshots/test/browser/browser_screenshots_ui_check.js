"use strict";

function checkElements(expectPresent, l) {
  for (let id of l) {
    is(!!document.getElementById(id), expectPresent, "element " + id + (expectPresent ? " is" : " is not") + " present");
  }
}

add_task(async function() {
  await promiseScreenshotsEnabled();

  registerCleanupFunction(async function() {
    await promiseScreenshotsReset();
  });

  let onPhoton = (typeof AppConstants.MOZ_PHOTON_THEME == "undefined") ||
                 AppConstants.MOZ_PHOTON_THEME;
  let id = onPhoton ? "pageAction-panel-screenshots" : "screenshots_mozilla_org-browser-action";

  await BrowserTestUtils.waitForCondition(
    () => document.getElementById(id),
    "Screenshots button should be present", 100, 100);

  checkElements(true, [id]);
});
