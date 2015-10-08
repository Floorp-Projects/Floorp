/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test the toolbar button states.
 */

"use strict";

Components.utils.import("resource://gre/modules/Promise.jsm", this);
const {LoopRoomsInternal} = Components.utils.import("resource:///modules/loop/LoopRooms.jsm", {});
Services.prefs.setBoolPref("loop.gettingStarted.seen", true);

const fxASampleToken = {
  token_type: "bearer",
  access_token: "1bad3e44b12f77a88fe09f016f6a37c42e40f974bc7a8b432bb0d2f0e37e1752",
  scope: "profile"
};

const fxASampleProfile = {
  email: "test@example.com",
  uid: "abcd1234"
};

registerCleanupFunction(function*() {
  MozLoopService.doNotDisturb = false;
  MozLoopServiceInternal.fxAOAuthProfile = null;
  yield MozLoopServiceInternal.clearError("testing");
  Services.prefs.clearUserPref("loop.gettingStarted.seen");
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
  yield MozLoopService.doNotDisturb = true;
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "disabled", "Check button is in disabled state");
  yield MozLoopService.doNotDisturb = false;
  Assert.notStrictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "disabled", "Check button is not in disabled state");
});

add_task(function* test_doNotDisturb_with_login() {
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "", "Check button is in default state");
  yield MozLoopService.doNotDisturb = true;
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "disabled", "Check button is in disabled state");
  MozLoopServiceInternal.fxAOAuthTokenData = fxASampleToken;
  MozLoopServiceInternal.fxAOAuthProfile = fxASampleProfile;
  yield MozLoopServiceInternal.notifyStatusChanged("login");
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "active", "Check button is in active state");
  yield loadLoopPanel();
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "disabled", "Check button is in disabled state after opening panel");
  LoopUI.panel.hidePopup();
  yield MozLoopService.doNotDisturb = false;
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "", "Check button is in default state");
  MozLoopServiceInternal.fxAOAuthTokenData = null;
  yield MozLoopServiceInternal.notifyStatusChanged();
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "", "Check button is in default state");
});

add_task(function* test_error() {
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "", "Check button is in default state");
  yield MozLoopServiceInternal.setError("testing", {});
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "error", "Check button is in error state");
  yield MozLoopServiceInternal.clearError("testing");
  Assert.notStrictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "error", "Check button is not in error state");
});

add_task(function* test_error_with_login() {
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "", "Check button is in default state");
  yield MozLoopServiceInternal.setError("testing", {});
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "error", "Check button is in error state");
  MozLoopServiceInternal.fxAOAuthProfile = fxASampleProfile;
  MozLoopServiceInternal.notifyStatusChanged("login");
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "error", "Check button is in error state");
  yield MozLoopServiceInternal.clearError("testing");
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "", "Check button is in default state");
  MozLoopServiceInternal.fxAOAuthProfile = null;
  MozLoopServiceInternal.notifyStatusChanged();
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "", "Check button is in default state");
});

add_task(function* test_active() {
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "", "Check button is in default state");
  MozLoopServiceInternal.fxAOAuthTokenData = fxASampleToken;
  MozLoopServiceInternal.fxAOAuthProfile = fxASampleProfile;
  yield MozLoopServiceInternal.notifyStatusChanged("login");
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "active", "Check button is in active state");
  yield loadLoopPanel();
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "", "Check button is in default state after opening panel");
  LoopUI.panel.hidePopup();
  MozLoopServiceInternal.fxAOAuthTokenData = null;
  MozLoopServiceInternal.notifyStatusChanged();
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "", "Check button is in default state");
});

add_task(function* test_room_participants() {
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "", "Check button is in default state");
  LoopRoomsInternal.rooms.set("test_room", {participants: [{displayName: "hugh", id: "008"}]});
  MozLoopServiceInternal.notifyStatusChanged();
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "active", "Check button is in active state");
  LoopRoomsInternal.rooms.set("test_room", {participants: []});
  MozLoopServiceInternal.notifyStatusChanged();
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "", "Check button is in default state");
  LoopRoomsInternal.rooms.delete("test_room");
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
  MozLoopService.setScreenShareState("1", true);
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "action", "Check button is in action state");
  MozLoopService.setScreenShareState("1", false);
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "", "Check button is in default state");
});

add_task(function* test_private_browsing_window() {
  let win = OpenBrowserWindow({ private: true });
  yield new Promise(resolve => {
    win.addEventListener("load", function listener() {
      win.removeEventListener("load", listener);
      resolve();
    });
  });

  let button = win.LoopUI.toolbarButton.node;
  Assert.ok(button, "Loop button should be present");
  Assert.ok(button.getAttribute("disabled"), "Disabled attribute should be set");

  win.close();
});
