/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the eyedropper command works

const TESTCASE_URI = TEST_BASE + "color-block.html";
const DIV_COLOR = "#0000FF";

function test() {
  return Task.spawn(spawnTest).then(finish, helpers.handleError);
}

function spawnTest() {
  let options = yield helpers.openTab(TESTCASE_URI);
  yield helpers.openToolbar(options);

  yield helpers.audit(options, [
    {
      setup: "eyedropper",
      check: {
        input: "eyedropper"
      },
      exec: { output: "" }
    },
  ]);

  yield inspectAndWaitForCopy();

  yield helpers.closeToolbar(options);
  yield helpers.closeTab(options);
}

function inspectAndWaitForCopy() {
  let deferred = promise.defer();

  waitForClipboard(DIV_COLOR, () => {
    inspectPage(); // setup: inspect the page
  }, deferred.resolve, deferred.reject);

  return deferred.promise;
}

function inspectPage() {
  let target = content.document.getElementById("test");
  let win = content.window;

  EventUtils.synthesizeMouse(target, 20, 20, { type: "mousemove" }, win);

  let dropper = EyedropperManager.getInstance(window);

  return dropperLoaded(dropper).then(() => {
    EventUtils.synthesizeMouse(target, 30, 30, { type: "mousemove" }, win);

    EventUtils.synthesizeMouse(target, 30, 30, {}, win);
  });
}
