/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const kButtonId = "test-tabbar-size-with-large-buttons";

function test() {
  registerCleanupFunction(cleanup);
  let titlebar = document.getElementById("titlebar");
  let originalHeight = titlebar.getBoundingClientRect().height;
  let button = document.createElement("toolbarbutton");
  button.id = kButtonId;
  button.setAttribute("style", "min-height: 100px");
  gNavToolbox.palette.appendChild(button);
  CustomizableUI.addWidgetToArea(kButtonId, CustomizableUI.AREA_TABSTRIP);
  let currentHeight = titlebar.getBoundingClientRect().height;
  ok(currentHeight > originalHeight, "Titlebar should have grown");
  CustomizableUI.removeWidgetFromArea(kButtonId);
  currentHeight = titlebar.getBoundingClientRect().height;
  is(currentHeight, originalHeight, "Titlebar should have gone back to its original size.");
}

function cleanup() {
  let btn = document.getElementById(kButtonId);
  if (btn) {
    btn.remove();
  }
}

