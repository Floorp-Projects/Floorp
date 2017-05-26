"use strict";

function checkElements(expectPresent, l) {
  for (let id of l) {
    is(!!document.getElementById(id), expectPresent, "element " + id + (expectPresent ? " is" : " is not") + " present");
  }
}

add_task(function*() {
  yield promiseScreenshotsEnabled();

  registerCleanupFunction(function* () {
    yield promiseScreenshotsReset();
  });

  yield BrowserTestUtils.waitForCondition(
    () => document.getElementById("screenshots_mozilla_org-browser-action"),
    "Screenshots button should be present", 100, 100);

  checkElements(true, ["screenshots_mozilla_org-browser-action"]);
});
