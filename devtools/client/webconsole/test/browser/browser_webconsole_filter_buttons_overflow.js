/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test the locations of the filter buttons in the Webconsole's Filter Bar.

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-console.html";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);
  const win = hud.browserWindow;
  const initialWindowWidth = win.outerWidth;

  info(
    "Check filter buttons are inline with filter input when window is large."
  );
  resizeWindow(1500, win);
  await waitForFilterBarLayout(hud, ".wide");
  ok(true, "The filter bar has the wide layout");

  info("Check filter buttons overflow when window is small.");
  resizeWindow(400, win);
  await waitForFilterBarLayout(hud, ".narrow");
  ok(true, "The filter bar has the narrow layout");

  info("Check that the filter bar layout changes when opening the sidebar");
  resizeWindow(750, win);
  await waitForFilterBarLayout(hud, ".wide");
  const onMessage = waitForMessageByType(hud, "world", ".console-api");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.console.log({ hello: "world" });
  });
  const { node } = await onMessage;
  const object = node.querySelector(".object-inspector .objectBox-object");
  info("Ctrl+click on an object to put it in the sidebar");
  const onSidebarShown = waitFor(() =>
    hud.ui.document.querySelector(".sidebar")
  );
  AccessibilityUtils.setEnv({
    // Component that renders an object handles keyboard interactions on the
    // container level.
    mustHaveAccessibleRule: false,
    interactiveRule: false,
    focusableRule: false,
    labelRule: false,
  });
  EventUtils.sendMouseEvent(
    {
      type: "click",
      [Services.appinfo.OS === "Darwin" ? "metaKey" : "ctrlKey"]: true,
    },
    object,
    hud.ui.window
  );
  AccessibilityUtils.resetEnv();
  const sidebar = await onSidebarShown;
  await waitForFilterBarLayout(hud, ".narrow");
  ok(true, "FilterBar layout changed when opening the sidebar");

  info("Check that filter bar layout changes when closing the sidebar");
  sidebar.querySelector(".sidebar-close-button").click();
  await waitForFilterBarLayout(hud, ".wide");

  info("Restore the original window size");
  await resizeWindow(initialWindowWidth, win);

  await closeTabAndToolbox();
});

function resizeWindow(width, win) {
  const onResize = once(win, "resize");
  win.resizeTo(width, win.outerHeight);
  info("Wait for window resize event");
  return onResize;
}

function waitForFilterBarLayout(hud, query) {
  return waitFor(() =>
    hud.ui.outputNode.querySelector(`.webconsole-filteringbar-wrapper${query}`)
  );
}
