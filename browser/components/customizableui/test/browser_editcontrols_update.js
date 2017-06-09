// This test checks that the edit command enabled state (cut/paste) is updated
// properly when the edit controls are on the toolbar, popup and not present.
// It also verifies that the performance optimiation implemented by
// updateEditUIVisibility in browser.js is applied.

let isMac = navigator.platform.indexOf("Mac") == 0;

function checkState(allowCut, desc, testWindow = window) {
  is(testWindow.document.getElementById("cmd_cut").getAttribute("disabled") == "true", !allowCut, desc + " - cut");
  is(testWindow.document.getElementById("cmd_paste").getAttribute("disabled") == "true", false, desc + " - paste");
}

// Add a special controller to the urlbar and browser to listen in on when
// commands are being updated. Return a promise that resolves when 'count'
// updates have occurred.
function expectCommandUpdate(count, testWindow = window) {
  return new Promise((resolve, reject) => {
    let overrideController = {
      supportsCommand(cmd) { return cmd == "cmd_delete"; },
      isCommandEnabled(cmd) {
        if (!count) {
          ok(false, "unexpected update");
          reject();
        }

        if (!--count) {
          testWindow.gURLBar.controllers.removeControllerAt(0, overrideController);
          testWindow.gBrowser.selectedBrowser.controllers.removeControllerAt(0, overrideController);
          resolve(true);
        }
      }
    };

    if (!count) {
      SimpleTest.executeSoon(() => {
        testWindow.gURLBar.controllers.removeControllerAt(0, overrideController);
        testWindow.gBrowser.selectedBrowser.controllers.removeControllerAt(0, overrideController);
        resolve(false);
      });
    }

    testWindow.gURLBar.controllers.insertControllerAt(0, overrideController);
    testWindow.gBrowser.selectedBrowser.controllers.insertControllerAt(0, overrideController);
  });
}

add_task(async function test_init() {
  // Put something on the clipboard to verify that the paste button is properly enabled during the test.
  let clipboardHelper = Cc["@mozilla.org/widget/clipboardhelper;1"].getService(Ci.nsIClipboardHelper);
  await new Promise(resolve => {
    SimpleTest.waitForClipboard("Sample", function() { clipboardHelper.copyString("Sample"); }, resolve);
  });

  // Open and close the panel first so that it is fully initialized.
  await PanelUI.show();
  let hiddenPromise = promisePanelHidden(window);
  PanelUI.hide();
  await hiddenPromise;
});

// Test updating when the panel is open with the edit-controls on the panel.
// Updates should occur.
add_task(async function test_panelui_opened() {
  gURLBar.focus();
  gURLBar.value = "test";

  await PanelUI.show();

  checkState(false, "Update when edit-controls is on panel and visible");

  let overridePromise = expectCommandUpdate(1);
  gURLBar.select();
  await overridePromise;

  checkState(true, "Update when edit-controls is on panel and selection changed");

  overridePromise = expectCommandUpdate(0);
  let hiddenPromise = promisePanelHidden(window);
  PanelUI.hide();
  await hiddenPromise;
  await overridePromise;

  // Check that updates do not occur after the panel has been closed.
  checkState(true, "Update when edit-controls is on panel and hidden");

  // Mac will update the enabled st1ate even when the panel is closed so that
  // main menubar shortcuts will work properly.
  overridePromise = expectCommandUpdate(isMac ? 1 : 0);
  gURLBar.select();
  await overridePromise;
  checkState(true, "Update when edit-controls is on panel, hidden and selection changed");
});

// Test updating when the edit-controls are moved to the toolbar.
add_task(async function test_panelui_customize_to_toolbar() {
  await startCustomizing();
  let navbar = document.getElementById("nav-bar");
  simulateItemDrag(document.getElementById("edit-controls"), navbar.customizationTarget);
  await endCustomizing();

  // updateEditUIVisibility should be called when customization ends but isn't. See bug 1359790.
  updateEditUIVisibility();

  // The URL bar may have been focused to begin with, which means
  // that subsequent calls to focus it won't result in command
  // updates, so we'll make sure to blur it.
  gURLBar.blur();

  let overridePromise = expectCommandUpdate(1);
  gURLBar.select();
  gURLBar.focus();
  gURLBar.value = "other";
  await overridePromise;
  checkState(false, "Update when edit-controls on toolbar and focused");

  overridePromise = expectCommandUpdate(1);
  gURLBar.select();
  await overridePromise;
  checkState(true, "Update when edit-controls on toolbar and selection changed");

  const kOverflowPanel = document.getElementById("widget-overflow");

  let originalWidth = window.outerWidth;
  registerCleanupFunction(async function() {
    kOverflowPanel.removeAttribute("animate");
    window.resizeTo(originalWidth, window.outerHeight);
    await waitForCondition(() => !navbar.hasAttribute("overflowing"));
    CustomizableUI.reset();
  });

  window.resizeTo(400, window.outerHeight);
  await waitForCondition(() => navbar.hasAttribute("overflowing"));

  // Mac will update the enabled state even when the buttons are overflowing,
  // so main menubar shortcuts will work properly.
  overridePromise = expectCommandUpdate(isMac ? 1 : 0);
  gURLBar.select();
  await overridePromise;
  checkState(true, "Update when edit-controls is on overflow panel, hidden and selection changed");

  // Check that we get an update if we select content while the panel is open.
  overridePromise = expectCommandUpdate(1);
  await navbar.overflowable.show();
  gURLBar.select();
  await overridePromise;

  // And that we don't (except on mac) when the panel is hidden.
  kOverflowPanel.hidePopup();
  overridePromise = expectCommandUpdate(isMac ? 1 : 0);
  gURLBar.select();
  await overridePromise;

  window.resizeTo(originalWidth, window.outerHeight);
  await waitForCondition(() => !navbar.hasAttribute("overflowing"));

  if (gPhotonStructure) {
    CustomizableUI.addWidgetToArea("edit-controls", CustomizableUI.AREA_FIXED_OVERFLOW_PANEL);
    // updateEditUIVisibility should be called when customization happens but isn't. See bug 1359790.
    updateEditUIVisibility();

    overridePromise = expectCommandUpdate(isMac ? 1 : 0);
    gURLBar.select();
    await overridePromise;

    // Check that we get an update if we select content while the panel is open.
    overridePromise = expectCommandUpdate(1);
    await navbar.overflowable.show();
    gURLBar.select();
    await overridePromise;

    // And that we don't (except on mac) when the panel is hidden.
    kOverflowPanel.hidePopup();
    overridePromise = expectCommandUpdate(isMac ? 1 : 0);
    gURLBar.select();
    await overridePromise;
  }
});

// Test updating when the edit-controls are moved to the palette.
add_task(async function test_panelui_customize_to_palette() {
  await startCustomizing();
  let palette = document.getElementById("customization-palette");
  simulateItemDrag(document.getElementById("edit-controls"), palette);
  await endCustomizing();

  // updateEditUIVisibility should be called when customization ends but isn't. See bug 1359790.
  updateEditUIVisibility();

  let overridePromise = expectCommandUpdate(isMac ? 1 : 0);
  gURLBar.focus();
  gURLBar.value = "other";
  gURLBar.select();
  await overridePromise;

  // If the UI isn't found, the command is set to be enabled.
  checkState(true, "Update when edit-controls is on palette, hidden and selection changed");
});

add_task(async function finish() {
  await resetCustomization();
});

// Test updating in the initial state when the edit-controls are on the panel but
// have not yet been created. This needs to be done in a new window to ensure that
// other tests haven't opened the panel.
add_task(async function test_initial_state() {
  let testWindow = await BrowserTestUtils.openNewBrowserWindow();
  await SimpleTest.promiseFocus(testWindow);

  let overridePromise = expectCommandUpdate(isMac, testWindow);

  testWindow.gURLBar.focus();
  testWindow.gURLBar.value = "test";

  await overridePromise;

  // Commands won't update when no edit UI is present. They default to being
  // enabled so that keyboard shortcuts will work. The real enabled state will
  // be checked when shortcut is pressed.
  checkState(!isMac, "No update when edit-controls is on panel and not visible", testWindow);

  await BrowserTestUtils.closeWindow(testWindow);
  await SimpleTest.promiseFocus(window);
});
