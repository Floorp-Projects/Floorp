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

add_task(async () => {
  function createSimpleGestureEvent(type, direction) {
    let event = document.createEvent("SimpleGestureEvent");
    event.initSimpleGestureEvent(
      type,
      false /* canBubble */,
      false /* cancelableArg */,
      window,
      0 /* detail */,
      0 /* screenX */,
      0 /* screenY */,
      0 /* clientX */,
      0 /* clientY */,
      false /* ctrlKey */,
      false /* altKey */,
      false /* shiftKey */,
      false /* metaKey */,
      0 /* button */,
      null /* relatedTarget */,
      0 /* allowedDirections */,
      direction
    );
    return event;
  }

  await SpecialPowers.pushPrefEnv({
    set: [["ui.swipeAnimationEnabled", false]],
  });

  // Open a new browser window and load two pages so that the browser can go
  // back but can't go forward.
  const newWindow = await BrowserTestUtils.openNewBrowserWindow({});

  // gHistroySwipeAnimation gets initialized in a requestIdleCallback so we need
  // to wait for the initialization.
  await TestUtils.waitForCondition(() => {
    return (
      // There's no explicit notification for the initialization, so we wait
      // until `isLTR` matches the browser locale state.
      newWindow.gHistorySwipeAnimation.isLTR != Services.locale.isAppLocaleRTL
    );
  });

  BrowserTestUtils.loadURI(newWindow.gBrowser.selectedBrowser, "about:mozilla");
  await BrowserTestUtils.browserLoaded(
    newWindow.gBrowser.selectedBrowser,
    false,
    "about:mozilla"
  );
  BrowserTestUtils.loadURI(newWindow.gBrowser.selectedBrowser, "about:about");
  await BrowserTestUtils.browserLoaded(
    newWindow.gBrowser.selectedBrowser,
    false,
    "about:about"
  );

  let event = createSimpleGestureEvent(
    "SwipeGestureMayStart",
    SimpleGestureEvent.DIRECTION_LEFT
  );
  newWindow.gGestureSupport._shouldDoSwipeGesture(event);

  // Assuming we are on LTR environment.
  is(
    event.allowedDirections,
    SimpleGestureEvent.DIRECTION_LEFT,
    "Allows only swiping to left, i.e. backward"
  );

  event = createSimpleGestureEvent(
    "SwipeGestureMayStart",
    SimpleGestureEvent.DIRECTION_RIGHT
  );
  newWindow.gGestureSupport._shouldDoSwipeGesture(event);
  is(
    event.allowedDirections,
    SimpleGestureEvent.DIRECTION_LEFT,
    "Allows only swiping to left, i.e. backward"
  );

  await BrowserTestUtils.closeWindow(newWindow);
});
