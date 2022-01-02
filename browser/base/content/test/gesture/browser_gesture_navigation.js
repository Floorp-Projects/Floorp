/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async () => {
  // Open a new browser window to make sure there is no navigation history.
  const newBrowser = await BrowserTestUtils.openNewBrowserWindow({});

  let event = {
    direction: SimpleGestureEvent.DIRECTION_LEFT,
  };
  ok(!newBrowser.gGestureSupport._shouldDoSwipeGesture(event));

  event = {
    direction: SimpleGestureEvent.DIRECTION_RIGHT,
  };
  ok(!newBrowser.gGestureSupport._shouldDoSwipeGesture(event));

  await BrowserTestUtils.closeWindow(newBrowser);
});
