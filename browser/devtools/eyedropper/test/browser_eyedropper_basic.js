/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TESTCASE_URI = TEST_BASE + "color-block.html";
const DIV_COLOR = "#0000FF";

/**
 * Test basic eyedropper widget functionality:
 *  - Opening eyedropper and pressing ESC closes the eyedropper
 *  - Opening eyedropper and clicking copies the center color
 */
function test() {
  addTab(TESTCASE_URI).then(testEscape);
}

function testEscape() {
  let dropper = new Eyedropper(window);

  dropper.once("destroy", (event) => {
    ok(true, "escape closed the eyedropper");

    // now test selecting a color
    testSelect();
  });

  inspectPage(dropper, false).then(pressESC);
}

function testSelect() {
  let dropper = new Eyedropper(window);

  dropper.once("select", (event, color) => {
    is(color, DIV_COLOR, "correct color selected");
  });

  // wait for DIV_COLOR to be copied to the clipboard then finish the test.
  waitForClipboard(DIV_COLOR, () => {
    inspectPage(dropper); // setup: inspect the page
  }, finish, finish);
}

/* Helpers */

function inspectPage(dropper, click=true) {
  dropper.open();

  let target = content.document.getElementById("test");
  let win = content.window;

  EventUtils.synthesizeMouse(target, 20, 20, { type: "mousemove" }, win);

  return dropperLoaded(dropper).then(() => {
    EventUtils.synthesizeMouse(target, 30, 30, { type: "mousemove" }, win);

    if (click) {
      EventUtils.synthesizeMouse(target, 30, 30, {}, win);
    }
  });
}

function pressESC() {
  EventUtils.synthesizeKey("VK_ESCAPE", { });
}
