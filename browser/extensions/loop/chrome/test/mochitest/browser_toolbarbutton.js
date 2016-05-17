/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test the toolbar button states.
 */

"use strict";

Components.utils.import("resource://gre/modules/Promise.jsm", this);
Services.prefs.setIntPref("loop.gettingStarted.latestFTUVersion", 2);

const fxASampleToken = {
  token_type: "bearer",
  access_token: "1bad3e44b12f77a88fe09f016f6a37c42e40f974bc7a8b432bb0d2f0e37e1752",
  scope: "profile"
};

const fxASampleProfile = {
  email: "test@example.com",
  uid: "abcd1234"
};

registerCleanupFunction(function* () {
  MozLoopService.doNotDisturb = false;
  MozLoopServiceInternal.fxAOAuthProfile = null;
  yield MozLoopServiceInternal.clearError("testing");
  Services.prefs.clearUserPref("loop.gettingStarted.latestFTUVersion");
});

add_task(function* test_LoopUI_getters() {
  Assert.ok(LoopUI.panel, "LoopUI panel element should be set");
  Assert.strictEqual(LoopUI.browser, null, "Browser element should not be there yet");

  // Load and show the Loop panel for the very first time this session.
  yield loadLoopPanel();
  Assert.ok(LoopUI.browser, "Browser element should be there");

  // Hide the panel.
  yield LoopUI.togglePanel();
});

add_task(function* test_doNotDisturb() {
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "", "Check button is in default state");
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("tooltiptext"), "Browse this page with a friend", "Check button has default tooltiptext");
  yield MozLoopService.doNotDisturb = true;
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "disabled", "Check button is in disabled state");
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("tooltiptext"), "Do not disturb", "Check button has disabled tooltiptext");
  yield MozLoopService.doNotDisturb = false;
  Assert.notStrictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "disabled", "Check button is not in disabled state");
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("tooltiptext"), "Browse this page with a friend", "Check button has default tooltiptext");
});

add_task(function* test_doNotDisturb_with_login() {
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "", "Check button is in default state");
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("tooltiptext"), "Browse this page with a friend", "Check button has default tooltiptext");
  yield MozLoopService.doNotDisturb = true;
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "disabled", "Check button is in disabled state");
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("tooltiptext"), "Do not disturb", "Check button has disabled tooltiptext");
  MozLoopServiceInternal.fxAOAuthTokenData = fxASampleToken;
  MozLoopServiceInternal.fxAOAuthProfile = fxASampleProfile;
  yield MozLoopServiceInternal.notifyStatusChanged("login");
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "active", "Check button is in active state");
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("tooltiptext"), "You are sharing your tabs", "Check button has active tooltiptext");
  yield loadLoopPanel();
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "disabled", "Check button is in disabled state after opening panel");
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("tooltiptext"), "Do not disturb", "Check button has disabled tooltiptext");
  LoopUI.panel.hidePopup();
  yield MozLoopService.doNotDisturb = false;
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "", "Check button is in default state");
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("tooltiptext"), "Browse this page with a friend", "Check button has default tooltiptext");
  MozLoopServiceInternal.fxAOAuthTokenData = null;
  yield MozLoopServiceInternal.notifyStatusChanged();
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "", "Check button is in default state");
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("tooltiptext"), "Browse this page with a friend", "Check button has default tooltiptext");
});

add_task(function* test_error() {
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "", "Check button is in default state");
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("tooltiptext"), "Browse this page with a friend", "Check button has default tooltiptext");
  yield MozLoopServiceInternal.setError("testing", {});
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "error", "Check button is in error state");
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("tooltiptext"), "Error!", "Check button has error tooltiptext");
  yield MozLoopServiceInternal.clearError("testing");
  Assert.notStrictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "error", "Check button is not in error state");
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("tooltiptext"), "Browse this page with a friend", "Check button has default tooltiptext");
});

add_task(function* test_error_with_login() {
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "", "Check button is in default state");
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("tooltiptext"), "Browse this page with a friend", "Check button has default tooltiptext");
  yield MozLoopServiceInternal.setError("testing", {});
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "error", "Check button is in error state");
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("tooltiptext"), "Error!", "Check button has error tooltiptext");
  MozLoopServiceInternal.fxAOAuthProfile = fxASampleProfile;
  MozLoopServiceInternal.notifyStatusChanged("login");
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "error", "Check button is in error state");
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("tooltiptext"), "Error!", "Check button has error tooltiptext");
  yield MozLoopServiceInternal.clearError("testing");
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "", "Check button is in default state");
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("tooltiptext"), "Browse this page with a friend", "Check button has default tooltiptext");
  MozLoopServiceInternal.fxAOAuthProfile = null;
  MozLoopServiceInternal.notifyStatusChanged();
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "", "Check button is in default state");
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("tooltiptext"), "Browse this page with a friend", "Check button has default tooltiptext");
});

add_task(function* test_active() {
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "", "Check button is in default state");
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("tooltiptext"), "Browse this page with a friend", "Check button has default tooltiptext");
  MozLoopServiceInternal.fxAOAuthTokenData = fxASampleToken;
  MozLoopServiceInternal.fxAOAuthProfile = fxASampleProfile;
  yield MozLoopServiceInternal.notifyStatusChanged("login");
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "active", "Check button is in active state");
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("tooltiptext"), "You are sharing your tabs", "Check button has active tooltiptext");
  yield loadLoopPanel();
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "", "Check button is in default state after opening panel");
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("tooltiptext"), "Browse this page with a friend", "Check button has default tooltiptext");
  LoopUI.panel.hidePopup();
  MozLoopServiceInternal.fxAOAuthTokenData = null;
  MozLoopServiceInternal.notifyStatusChanged();
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "", "Check button is in default state");
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("tooltiptext"), "Browse this page with a friend", "Check button has default tooltiptext");
});

add_task(function* test_room_participants() {
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "", "Check button is in default state");
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("tooltiptext"), "Browse this page with a friend", "Check button has default tooltiptext");
  let roomsCache = new Map([["test_room", { participants: [{ displayName: "hugh", id: "008", owner: true }] }]]);
  LoopRooms._setRoomsCache(roomsCache);
  MozLoopServiceInternal.notifyStatusChanged();
  // Since we're changing the rooms map directly, we're expecting it to be a synchronous operation.
  // But Promises have the inherent property of then-ables being async so even though the operation returns immediately,
  // because the cache is hit, the promise won't be resolved until after the next tick.
  // And that's what the line below does, waits until the next tick
  yield new Promise(resolve => executeSoon(resolve));
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "active", "Check button is in active state");
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("tooltiptext"), "You are sharing your tabs", "Check button has active tooltiptext");
  roomsCache.set("test_room", { participants: [{ displayName: "hugh", id: "008", owner: false }] });
  LoopRooms._setRoomsCache(roomsCache);
  MozLoopServiceInternal.notifyStatusChanged();
  yield new Promise(resolve => executeSoon(resolve));
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "active", "Check button is in active state");
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("tooltiptext"), "Someone is waiting for you", "Check button has participantswaiting tooltiptext");
  roomsCache.set("test_room", { participants: [] });
  LoopRooms._setRoomsCache(roomsCache);
  MozLoopServiceInternal.notifyStatusChanged();
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "", "Check button is in default state");
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("tooltiptext"), "Browse this page with a friend", "Check button has default tooltiptext");
  LoopRooms._setRoomsCache();
});

add_task(function* test_panelToggle_on_click() {
  // Since we _know_ the first click on the button will open the panel, we'll
  // open it using the test utility and check the correct state by clicking the
  // button. This should hide the panel.
  // If we'd open the panel with a simulated click on the button, we won't know
  // for sure when the panel has opened, because the event loop spins a few times
  // in the mean time.
  yield loadLoopPanel();
  Assert.strictEqual(LoopUI.panel.state, "open", "Panel should be open");
  // The panel should now be visible. Clicking the button should hide it.
  LoopUI.toolbarButton.node.click();
  Assert.strictEqual(LoopUI.panel.state, "closed", "Panel should be closed");
});

add_task(function* test_screen_share() {
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "", "Check button is in default state");
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("tooltiptext"), "Browse this page with a friend", "Check button has default tooltiptext");
  MozLoopService.setScreenShareState("1", true);
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "action", "Check button is in action state");
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("tooltiptext"), "You are sharing your screen", "Check button has sharingscreen tooltiptext");
  MozLoopService.setScreenShareState("1", false);
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "", "Check button is in default state");
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("tooltiptext"), "Browse this page with a friend", "Check button has default tooltiptext");
});

add_task(function* test_private_browsing_window() {
  let win = yield BrowserTestUtils.openNewBrowserWindow({ private: true });
  let button = win.LoopUI.toolbarButton.node;
  Assert.ok(button, "Loop button should be present");
  Assert.ok(button.getAttribute("disabled"), "Disabled attribute should be set");

  yield BrowserTestUtils.closeWindow(win);
});
