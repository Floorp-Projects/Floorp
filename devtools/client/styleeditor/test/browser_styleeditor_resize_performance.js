/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This is a performance test designed to check we are not redrawing the UI too many times
 * after resizing the window, when the styleeditor displays mediaqueries which are source
 * mapped.
 * See Bug 1453044 for more details.
 */

const TESTCASE_URI = TEST_BASE_HTTP + "many-media-rules-sourcemaps/index.html";

// Maximum delay allowed between two at-rules-list-changed events.
const EVENTS_DELAY = 2000;

// The window resize will still trigger several resize events which will lead to several
// UI updates. Arbitrary maximum number of events allowed to be fired for a single resize.
// This used to be > 100 events for this test case.
const MAX_EVENTS = 10;

add_task(async function () {
  const { toolbox, ui } = await openStyleEditorForURL(TESTCASE_URI);

  const win = toolbox.win.parent;
  const originalWidth = win.outerWidth;
  const originalHeight = win.outerHeight;

  // Ensure the window is above 500px wide for @media (min-width: 500px)
  if (originalWidth < 500) {
    info("Window is too small for the test, resize it to > 800px width");
    const onMediaListChanged = waitForManyEvents(ui, EVENTS_DELAY);
    await resizeWindow(800, ui, win);
    info("Wait for at-rules-list-changed events to settle");
    await onMediaListChanged;
  }

  info(
    "Resize the window to stop matching media queries, and trigger the UI updates"
  );
  const onMediaListChanged = waitForManyEvents(ui, win, EVENTS_DELAY);
  await resizeWindow(400, ui, win);
  const eventsCount = await onMediaListChanged;

  ok(
    eventsCount < MAX_EVENTS,
    `Too many events fired (expected less than ${MAX_EVENTS}, got ${eventsCount})`
  );

  win.resizeTo(originalWidth, originalHeight);
});

/**
 * Resize the window to the provided width.
 */
async function resizeWindow(width, ui, win) {
  const onResize = once(win, "resize");
  win.resizeTo(width, win.outerHeight);
  info("Wait for window resize event");
  await onResize;
}
