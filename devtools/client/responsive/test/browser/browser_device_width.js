/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL =
  'data:text/html;charset=utf-8,<iframe id="subframe" ' +
  'width="200" height="200"></iframe>';

addRDMTask(TEST_URL, async function({ ui, manager }) {
  await pushPref("devtools.responsive.metaViewport.enabled", true);

  ok(ui, "An instance of the RDM should be attached to the tab.");
  await setViewportSizeAndAwaitReflow(ui, manager, 110, 500);

  info("Checking initial width/height properties.");
  await doInitialChecks(ui, 110);

  info("Checking initial width/height with meta viewport on");
  await setTouchAndMetaViewportSupport(ui, true);
  await doInitialChecks(ui, 440);
  await setTouchAndMetaViewportSupport(ui, false);

  info("Changing the RDM size");
  await setViewportSizeAndAwaitReflow(ui, manager, 90, 500);

  info("Checking for screen props");
  await checkScreenProps(ui);

  info("Checking for screen props with meta viewport on");
  await setTouchAndMetaViewportSupport(ui, true);
  await checkScreenProps(ui);
  await setTouchAndMetaViewportSupport(ui, false);

  info("Checking for subframe props");
  await checkSubframeProps(ui);

  info("Checking for subframe props with meta viewport on");
  await setTouchAndMetaViewportSupport(ui, true);
  await checkSubframeProps(ui);
  await setTouchAndMetaViewportSupport(ui, false);

  info("Changing the RDM size using input keys");
  await setViewportSizeWithInputKeys(ui);

  info("Checking for screen props once again.");
  await checkScreenProps2(ui);
});

async function setViewportSizeWithInputKeys(ui) {
  const width = 320,
    height = 500;
  let resized = waitForViewportResizeTo(ui, width, height);
  ui.setViewportSize({ width, height });
  await resized;

  const dimensions = ui.toolWindow.document.querySelectorAll(
    ".viewport-dimension-input"
  );

  // Increase width value to 420 by using the Up arrow key
  resized = waitForViewportResizeTo(ui, 420, height);
  dimensions[0].focus();
  for (let i = 1; i <= 100; i++) {
    EventUtils.synthesizeKey("KEY_ArrowUp");
  }
  await resized;

  // Resetting width value back to 320 using `Shift + Down` arrow
  resized = waitForViewportResizeTo(ui, width, height);
  dimensions[0].focus();
  for (let i = 1; i <= 10; i++) {
    EventUtils.synthesizeKey("KEY_ArrowDown", { shiftKey: true });
  }
  await resized;

  // Increase height value to 600 by using `PageUp + Shift` key
  resized = waitForViewportResizeTo(ui, width, 600);
  dimensions[1].focus();
  EventUtils.synthesizeKey("KEY_PageUp", { shiftKey: true });
  await resized;

  // Resetting height value back to 500 by using `PageDown + Shift` key
  resized = waitForViewportResizeTo(ui, width, height);
  dimensions[1].focus();
  EventUtils.synthesizeKey("KEY_PageDown", { shiftKey: true });
  await resized;
}

async function doInitialChecks(ui, expectedInnerWidth) {
  const {
    innerWidth,
    matchesMedia,
    outerHeight,
    outerWidth,
  } = await grabContentInfo(ui);
  is(innerWidth, expectedInnerWidth, "inner width should be as expected");
  is(outerWidth, 110, "device's outerWidth should be 110px");
  is(outerHeight, 500, "device's outerHeight should be 500px");
  isnot(
    window.outerHeight,
    outerHeight,
    "window.outerHeight should not be the size of the device's outerHeight"
  );
  isnot(
    window.outerWidth,
    outerWidth,
    "window.outerWidth should not be the size of the device's outerWidth"
  );
  ok(!matchesMedia, "media query shouldn't match.");
}

async function checkScreenProps(ui) {
  const { matchesMedia, screen } = await grabContentInfo(ui);
  ok(matchesMedia, "media query should match");
  isnot(
    window.screen.width,
    screen.width,
    "screen.width should not be the size of the screen."
  );
  is(screen.width, 90, "screen.width should be the page width");
  is(screen.height, 500, "screen.height should be the page height");
}

async function checkScreenProps2(ui) {
  const { screen } = await grabContentInfo(ui);
  isnot(
    window.screen.width,
    screen.width,
    "screen.width should not be the size of the screen."
  );
}

async function checkSubframeProps(ui) {
  const { outerWidth, matchesMedia, screen } = await grabContentSubframeInfo(
    ui
  );
  is(outerWidth, 90, "subframe outerWidth should be 90px");
  ok(matchesMedia, "subframe media query should match");
  is(screen.width, 90, "subframe screen.width should be the page width");
  is(screen.height, 500, "subframe screen.height should be the page height");
}

function grabContentInfo(ui) {
  return ContentTask.spawn(ui.getViewportBrowser(), {}, async function() {
    return {
      screen: {
        width: content.screen.width,
        height: content.screen.height,
      },
      innerWidth: content.innerWidth,
      matchesMedia: content.matchMedia("(max-device-width:100px)").matches,
      outerHeight: content.outerHeight,
      outerWidth: content.outerWidth,
    };
  });
}

function grabContentSubframeInfo(ui) {
  return ContentTask.spawn(ui.getViewportBrowser(), {}, async function() {
    const subframe = content.document.getElementById("subframe");
    const win = subframe.contentWindow;
    return {
      screen: {
        width: win.screen.width,
        height: win.screen.height,
      },
      innerWidth: win.innerWidth,
      matchesMedia: win.matchMedia("(max-device-width:100px)").matches,
      outerHeight: win.outerHeight,
      outerWidth: win.outerWidth,
    };
  });
}
