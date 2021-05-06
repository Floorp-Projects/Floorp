/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function regular_mode() {
  let className;
  await showAndWaitForDialog(async win => {
    await BrowserTestUtils.waitForEvent(win, "ready");
    ({ className } = win.document.body);
    win.close();
  });

  Assert.equal(className, "", "No classes set on body");
});

add_task(async function compact_mode() {
  // Shrink the window for this test.
  const { outerHeight, outerWidth } = window;
  window.resizeTo(outerWidth, 500);

  let className;
  await showAndWaitForDialog(async win => {
    await BrowserTestUtils.waitForEvent(win, "ready");
    ({ className } = win.document.body);
    win.close();
  });

  Assert.equal(className, "compact", "Set class on body");

  window.resizeTo(outerWidth, outerHeight);
});
