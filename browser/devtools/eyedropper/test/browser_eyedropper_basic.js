/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TESTCASE_URI = CHROME_URL_ROOT + "color-block.html";
const DIV_COLOR = "#0000FF";

/**
 * Test basic eyedropper widget functionality:
 *  - Opening eyedropper and pressing ESC closes the eyedropper
 *  - Opening eyedropper and clicking copies the center color
 */
add_task(function*() {
  yield addTab(TESTCASE_URI);

  info("added tab");

  yield testEscape();

  info("testing selecting a color");

  yield testSelect();
});

function* testEscape() {
  let dropper = new Eyedropper(window);

  yield inspectPage(dropper, false);

  let destroyed = dropper.once("destroy");
  pressESC();
  yield destroyed;

  ok(true, "escape closed the eyedropper");
}

function* testSelect() {
  let dropper = new Eyedropper(window);

  let selected = dropper.once("select");
  let copied = waitForClipboard(() => {}, DIV_COLOR);

  yield inspectPage(dropper);

  let color = yield selected;
  is(color, DIV_COLOR, "correct color selected");

  // wait for DIV_COLOR to be copied to the clipboard
  yield copied;
}

/* Helpers */

function* inspectPage(dropper, click=true) {
  yield dropper.open();

  info("dropper opened");

  let target = document.documentElement;
  let win = window;

  // get location of the <div> in the content, offset from browser window
  let box = gBrowser.selectedBrowser.getBoundingClientRect();
  let x = box.left + 100;
  let y = box.top + 100;

  EventUtils.synthesizeMouse(target, x, y, { type: "mousemove" }, win);

  yield dropperLoaded(dropper);

  EventUtils.synthesizeMouse(target, x + 10, y + 10, { type: "mousemove" }, win);

  if (click) {
    EventUtils.synthesizeMouse(target, x + 10, y + 10, {}, win);
  }
}

function pressESC() {
  EventUtils.synthesizeKey("VK_ESCAPE", { });
}
