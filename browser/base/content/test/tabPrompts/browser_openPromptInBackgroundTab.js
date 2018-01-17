"use strict";

const ROOT = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://example.com/");
let pageWithAlert = ROOT + "openPromptOffTimeout.html";

registerCleanupFunction(function() {
  Services.perms.removeAll();
});

/*
 * This test opens a tab that alerts when it is hidden. We then switch away
 * from the tab, and check that by default the tab is not automatically
 * re-selected. We also check that a checkbox appears in the alert that allows
 * the user to enable this automatically re-selecting. We then check that
 * checking the checkbox does actually enable that behaviour.
 */
add_task(async function() {
  let firstTab = gBrowser.selectedTab;
  // load page that opens prompt when page is hidden
  let openedTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, pageWithAlert, true);
  let openedTabGotAttentionPromise = BrowserTestUtils.waitForAttribute("attention", openedTab, "true");
  // switch away from that tab again - this triggers the alert.
  await BrowserTestUtils.switchTab(gBrowser, firstTab);
  // ... but that's async on e10s...
  await openedTabGotAttentionPromise;
  // check for attention attribute
  is(openedTab.getAttribute("attention"), "true", "Tab with alert should have 'attention' attribute.");
  ok(!openedTab.selected, "Tab with alert should not be selected");

  // switch tab back, and check the checkbox is displayed:
  await BrowserTestUtils.switchTab(gBrowser, openedTab);
  // check the prompt is there, and the extra row is present
  let prompts = openedTab.linkedBrowser.parentNode.querySelectorAll("tabmodalprompt");
  is(prompts.length, 1, "There should be 1 prompt");
  let ourPrompt = prompts[0];
  let row = ourPrompt.querySelector("row");
  ok(row, "Should have found the row with our checkbox");
  let checkbox = row.querySelector("checkbox[label*='example.com']");
  ok(checkbox, "The checkbox should be there");
  ok(!checkbox.checked, "Checkbox shouldn't be checked");
  // tick box and accept dialog
  checkbox.checked = true;
  ourPrompt.onButtonClick(0);
  // Wait for that click to actually be handled completely.
  await new Promise(function(resolve) {
    Services.tm.dispatchToMainThread(resolve);
  });
  // check permission is set
  let ps = Services.perms;
  is(ps.ALLOW_ACTION, ps.testPermission(makeURI(pageWithAlert), "focus-tab-by-prompt"),
     "Tab switching should now be allowed");

  // Check if the control center shows the correct permission.
  let shown = BrowserTestUtils.waitForEvent(gIdentityHandler._identityPopup, "popupshown");
  gIdentityHandler._identityBox.click();
  await shown;
  let labelText = SitePermissions.getPermissionLabel("focus-tab-by-prompt");
  let permissionsList = document.getElementById("identity-popup-permission-list");
  let label = permissionsList.querySelector(".identity-popup-permission-label");
  is(label.textContent, labelText);
  gIdentityHandler._identityPopup.hidePopup();

  // Check if the identity icon signals granted permission.
  ok(gIdentityHandler._identityBox.classList.contains("grantedPermissions"),
    "identity-box signals granted permissions");

  let openedTabSelectedPromise = BrowserTestUtils.waitForAttribute("selected", openedTab, "true");
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

  await BrowserTestUtils.removeTab(openedTab);
});
