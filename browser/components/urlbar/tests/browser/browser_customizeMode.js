/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This test checks that the left/right arrow keys and home/end keys work in
// the urlbar after customize mode starts and ends.

"use strict";

add_task(async function test() {
  let win = await BrowserTestUtils.openNewBrowserWindow();
  await startCustomizing(win);
  await endCustomizing(win);

  let urlbar = win.gURLBar;

  let value = "example";
  urlbar.value = value;
  urlbar.focus();
  urlbar.selectionEnd = value.length;
  urlbar.selectionStart = value.length;

  // left
  EventUtils.synthesizeKey("KEY_ArrowLeft", {}, win);
  Assert.equal(urlbar.selectionStart, value.length - 1);
  Assert.equal(urlbar.selectionEnd, value.length - 1);

  // home
  if (AppConstants.platform == "macosx") {
    EventUtils.synthesizeKey("KEY_ArrowLeft", { metaKey: true }, win);
  } else {
    EventUtils.synthesizeKey("KEY_Home", {}, win);
  }
  Assert.equal(urlbar.selectionStart, 0);
  Assert.equal(urlbar.selectionEnd, 0);

  // right
  EventUtils.synthesizeKey("KEY_ArrowRight", {}, win);
  Assert.equal(urlbar.selectionStart, 1);
  Assert.equal(urlbar.selectionEnd, 1);

  // end
  if (AppConstants.platform == "macosx") {
    EventUtils.synthesizeKey("KEY_ArrowRight", { metaKey: true }, win);
  } else {
    EventUtils.synthesizeKey("KEY_End", {}, win);
  }
  Assert.equal(urlbar.selectionStart, value.length);
  Assert.equal(urlbar.selectionEnd, value.length);

  await BrowserTestUtils.closeWindow(win);
});

async function startCustomizing(win = window) {
  if (win.document.documentElement.getAttribute("customizing") != "true") {
    let eventPromise =
      BrowserTestUtils.waitForEvent(win.gNavToolbox, "customizationready");
    win.gCustomizeMode.enter();
    await eventPromise;
  }
}

async function endCustomizing(win = window) {
  if (win.document.documentElement.getAttribute("customizing") == "true") {
    let eventPromise =
      BrowserTestUtils.waitForEvent(win.gNavToolbox, "aftercustomization");
    win.gCustomizeMode.exit();
    await eventPromise;
  }
}
