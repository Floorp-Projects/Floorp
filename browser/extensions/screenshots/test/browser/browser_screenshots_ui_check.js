"use strict";

/* global add_task, is, promiseScreenshotsEnabled, promiseScreenshotsReset,
   registerCleanupFunction */

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

  checkElements(true, ["screenshots_mozilla_org-browser-action"]);
});
