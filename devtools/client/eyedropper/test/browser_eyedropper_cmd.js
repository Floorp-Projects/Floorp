/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the eyedropper command works

const TESTCASE_URI = CHROME_URL_ROOT + "color-block.html";
const DIV_COLOR = "#0000FF";

function test() {
  return Task.spawn(spawnTest).then(finish, helpers.handleError);
}

function* spawnTest() {
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
  let copied = waitForClipboard(() => {}, DIV_COLOR);
  let ready = inspectPage(); // resolves once eyedropper is destroyed

  return Promise.all([copied, ready]);
}

function inspectPage() {
  let target = document.documentElement;
  let win = window;

  // get location of the <div> in the content, offset from browser window
  let box = gBrowser.selectedBrowser.getBoundingClientRect();
  let x = box.left + 100;
  let y = box.top + 100;

  let dropper = EyedropperManager.getInstance(window);

  return dropperStarted(dropper).then(() => {
    EventUtils.synthesizeMouse(target, x, y, { type: "mousemove" }, win);

    return dropperLoaded(dropper).then(() => {
      EventUtils.synthesizeMouse(target, x + 10, y + 10, { type: "mousemove" }, win);

      EventUtils.synthesizeMouse(target, x + 10, y + 10, {}, win);
      return dropper.once("destroy");
    });
  });
}
