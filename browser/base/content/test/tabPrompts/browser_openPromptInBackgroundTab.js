"use strict";

const { PermissionTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PermissionTestUtils.sys.mjs"
);

const ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://example.com/"
);
let pageWithAlert = ROOT + "openPromptOffTimeout.html";

registerCleanupFunction(function () {
  Services.perms.removeAll();
});

/*
 * This test opens a tab that alerts when it is hidden. We then switch away
 * from the tab, and check that by default the tab is not automatically
 * re-selected. We also check that a checkbox appears in the alert that allows
 * the user to enable this automatically re-selecting. We then check that
 * checking the checkbox does actually enable that behaviour.
 */
add_task(async function test_old_modal_ui() {
  // We're intentionally testing the old modal mechanism, so disable the new one.
  await SpecialPowers.pushPrefEnv({
    set: [["prompts.contentPromptSubDialog", false]],
  });

  let firstTab = gBrowser.selectedTab;
  // load page that opens prompt when page is hidden
  let openedTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    pageWithAlert,
    true
  );
  let openedTabGotAttentionPromise = BrowserTestUtils.waitForAttribute(
    "attention",
    openedTab
  );
  // switch away from that tab again - this triggers the alert.
  await BrowserTestUtils.switchTab(gBrowser, firstTab);
  // ... but that's async on e10s...
  await openedTabGotAttentionPromise;
  // check for attention attribute
  is(
    openedTab.hasAttribute("attention"),
    true,
    "Tab with alert should have 'attention' attribute."
  );
  ok(!openedTab.selected, "Tab with alert should not be selected");

  // switch tab back, and check the checkbox is displayed:
  await BrowserTestUtils.switchTab(gBrowser, openedTab);
  // check the prompt is there, and the extra row is present
  let promptElements =
    openedTab.linkedBrowser.parentNode.querySelectorAll("tabmodalprompt");
  is(promptElements.length, 1, "There should be 1 prompt");
  let ourPromptElement = promptElements[0];
  let checkbox = ourPromptElement.querySelector(
    "checkbox[label*='example.com']"
  );
  ok(checkbox, "The checkbox should be there");
  ok(!checkbox.checked, "Checkbox shouldn't be checked");
  // tick box and accept dialog
  checkbox.checked = true;
  let ourPrompt =
    openedTab.linkedBrowser.tabModalPromptBox.getPrompt(ourPromptElement);
  ourPrompt.onButtonClick(0);
  // Wait for that click to actually be handled completely.
  await new Promise(function (resolve) {
    Services.tm.dispatchToMainThread(resolve);
  });
  // check permission is set
  is(
    Services.perms.ALLOW_ACTION,
    PermissionTestUtils.testPermission(pageWithAlert, "focus-tab-by-prompt"),
    "Tab switching should now be allowed"
  );

  // Check if the control center shows the correct permission.
  let shown = BrowserTestUtils.waitForEvent(
    window,
    "popupshown",
    true,
    event => event.target == gPermissionPanel._permissionPopup
  );
  gPermissionPanel._identityPermissionBox.click();
  await shown;
  let labelText = SitePermissions.getPermissionLabel("focus-tab-by-prompt");
  let permissionsList = document.getElementById(
    "permission-popup-permission-list"
  );
  let label = permissionsList.querySelector(
    ".permission-popup-permission-label"
  );
  is(label.textContent, labelText);
  gPermissionPanel._permissionPopup.hidePopup();

  // Check if the identity icon signals granted permission.
  ok(
    gPermissionPanel._identityPermissionBox.hasAttribute("hasPermissions"),
    "identity-box signals granted permissions"
  );

  let openedTabSelectedPromise = BrowserTestUtils.waitForAttribute(
    "selected",
    openedTab,
    "true"
  );
  // switch to other tab again
  await BrowserTestUtils.switchTab(gBrowser, firstTab);

  // This is sync in non-e10s, but in e10s we need to wait for this, so yield anyway.
  // Note that the switchTab promise doesn't actually guarantee anything about *which*
  // tab ends up as selected when its event fires, so using that here wouldn't work.
  await openedTabSelectedPromise;
  // should be switched back
  ok(openedTab.selected, "Ta-dah, the other tab should now be selected again!");

  // In e10s, with the conformant promise scheduling, we have to wait for next tick
  // to ensure that the prompt is open before removing the opened tab, because the
  // promise callback of 'openedTabSelectedPromise' could be done at the middle of
  // RemotePrompt.openTabPrompt() while 'DOMModalDialogClosed' event is fired.
  await TestUtils.waitForTick();

  BrowserTestUtils.removeTab(openedTab);
});

add_task(async function test_new_modal_ui() {
  // We're intentionally testing the new modal mechanism, so make sure it's enabled.
  await SpecialPowers.pushPrefEnv({
    set: [["prompts.contentPromptSubDialog", true]],
  });

  // Make sure we clear the focus tab permission set in the previous test
  PermissionTestUtils.remove(pageWithAlert, "focus-tab-by-prompt");

  let firstTab = gBrowser.selectedTab;
  // load page that opens prompt when page is hidden
  let openedTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    pageWithAlert,
    true
  );
  let openedTabGotAttentionPromise = BrowserTestUtils.waitForAttribute(
    "attention",
    openedTab
  );
  // switch away from that tab again - this triggers the alert.
  await BrowserTestUtils.switchTab(gBrowser, firstTab);
  // ... but that's async on e10s...
  await openedTabGotAttentionPromise;
  // check for attention attribute
  is(
    openedTab.hasAttribute("attention"),
    true,
    "Tab with alert should have 'attention' attribute."
  );
  ok(!openedTab.selected, "Tab with alert should not be selected");

  // switch tab back, and check the checkbox is displayed:
  await BrowserTestUtils.switchTab(gBrowser, openedTab);
  // check the prompt is there
  let promptElements = openedTab.linkedBrowser.parentNode.querySelectorAll(
    ".content-prompt-dialog"
  );

  let dialogBox = gBrowser.getTabDialogBox(openedTab.linkedBrowser);
  let contentPromptManager = dialogBox.getContentDialogManager();
  is(promptElements.length, 1, "There should be 1 prompt");
  is(
    contentPromptManager._dialogs.length,
    1,
    "Content prompt manager should have 1 dialog box."
  );

  // make sure the checkbox appears and that the permission for allowing tab switching
  // is set when the checkbox is tickted and the dialog is accepted
  let dialog = contentPromptManager._dialogs[0];

  await dialog._dialogReady;

  let dialogDoc = dialog._frame.contentWindow.document;
  let checkbox = dialogDoc.querySelector("checkbox[label*='example.com']");
  let button = dialogDoc.querySelector("#commonDialog").getButton("accept");

  ok(checkbox, "The checkbox should be there");
  ok(!checkbox.checked, "Checkbox shouldn't be checked");

  // tick box and accept dialog
  checkbox.checked = true;
  button.click();
  // Wait for that click to actually be handled completely.
  await new Promise(function (resolve) {
    Services.tm.dispatchToMainThread(resolve);
  });

  ok(!contentPromptManager._dialogs.length, "Dialog should be closed");

  // check permission is set
  is(
    Services.perms.ALLOW_ACTION,
    PermissionTestUtils.testPermission(pageWithAlert, "focus-tab-by-prompt"),
    "Tab switching should now be allowed"
  );

  // Check if the control center shows the correct permission.
  let shown = BrowserTestUtils.waitForEvent(
    window,
    "popupshown",
    true,
    event => event.target == gPermissionPanel._permissionPopup
  );
  gPermissionPanel._identityPermissionBox.click();
  await shown;
  let labelText = SitePermissions.getPermissionLabel("focus-tab-by-prompt");
  let permissionsList = document.getElementById(
    "permission-popup-permission-list"
  );
  let label = permissionsList.querySelector(
    ".permission-popup-permission-label"
  );
  is(label.textContent, labelText);
  gPermissionPanel.hidePopup();

  // Check if the identity icon signals granted permission.
  ok(
    gPermissionPanel._identityPermissionBox.hasAttribute("hasPermissions"),
    "identity-permission-box signals granted permissions"
  );

  let openedTabSelectedPromise = BrowserTestUtils.waitForAttribute(
    "selected",
    openedTab,
    "true"
  );

  // switch to other tab again
  await BrowserTestUtils.switchTab(gBrowser, firstTab);

  // This is sync in non-e10s, but in e10s we need to wait for this, so yield anyway.
  // Note that the switchTab promise doesn't actually guarantee anything about *which*
  // tab ends up as selected when its event fires, so using that here wouldn't work.
  await openedTabSelectedPromise;

  Assert.strictEqual(contentPromptManager._dialogs.length, 1, "Dialog opened.");
  dialog = contentPromptManager._dialogs[0];
  await dialog._dialogReady;

  // should be switched back
  ok(openedTab.selected, "Ta-dah, the other tab should now be selected again!");

  // In e10s, with the conformant promise scheduling, we have to wait for next tick
  // to ensure that the prompt is open before removing the opened tab, because the
  // promise callback of 'openedTabSelectedPromise' could be done at the middle of
  // RemotePrompt.openTabPrompt() while 'DOMModalDialogClosed' event is fired.
  // await TestUtils.waitForTick();

  await BrowserTestUtils.removeTab(openedTab);
});
