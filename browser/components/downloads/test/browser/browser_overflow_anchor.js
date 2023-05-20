/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

registerCleanupFunction(async function () {
  // Clean up when the test finishes.
  await task_resetState();
});

/**
 * Make sure the downloads button and indicator overflows into the nav-bar
 * chevron properly, and then when those buttons are clicked in the overflow
 * panel that the downloads panel anchors to the chevron`s icon.
 */
add_task(async function test_overflow_anchor() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.download.autohideButton", false]],
  });
  // Ensure that state is reset in case previous tests didn't finish.
  await task_resetState();

  // The downloads button should not be overflowed to begin with.
  let button = CustomizableUI.getWidget("downloads-button").forWindow(window);
  ok(!button.overflowed, "Downloads button should not be overflowed.");
  is(
    button.node.getAttribute("cui-areatype"),
    "toolbar",
    "Button should know it's in the toolbar"
  );

  await gCustomizeMode.addToPanel(button.node);

  let promise = promisePanelOpened();
  EventUtils.sendMouseEvent({ type: "mousedown", button: 0 }, button.node);
  info("waiting for panel to open");
  await promise;

  let panel = DownloadsPanel.panel;
  let chevron = document.getElementById("nav-bar-overflow-button");

  is(
    panel.anchorNode,
    chevron.icon,
    "Panel should be anchored to the chevron`s icon."
  );

  DownloadsPanel.hidePanel();

  gCustomizeMode.addToToolbar(button.node);

  // Now try opening the panel again.
  promise = promisePanelOpened();
  EventUtils.sendMouseEvent({ type: "mousedown", button: 0 }, button.node);
  await promise;

  let downloadsAnchor = button.node.badgeStack;
  is(panel.anchorNode, downloadsAnchor);

  DownloadsPanel.hidePanel();
});
