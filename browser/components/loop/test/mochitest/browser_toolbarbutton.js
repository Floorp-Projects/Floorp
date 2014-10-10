/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test the toolbar button states.
 */

"use strict";

registerCleanupFunction(function*() {
  MozLoopService.doNotDisturb = false;
  MozLoopServiceInternal.fxAOAuthProfile = null;
  yield MozLoopServiceInternal.clearError("testing");
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
  MozLoopServiceInternal.fxAOAuthTokenData = {token_type:"bearer",access_token:"1bad3e44b12f77a88fe09f016f6a37c42e40f974bc7a8b432bb0d2f0e37e1752",scope:"profile"};
  MozLoopServiceInternal.fxAOAuthProfile = {email: "test@example.com", uid: "abcd1234"};
  yield MozLoopServiceInternal.notifyStatusChanged("login");
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "active", "Check button is in active state");
  yield loadLoopPanel();
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "disabled", "Check button is in disabled state after opening panel");
  let loopPanel = document.getElementById("loop-notification-panel");
  loopPanel.hidePopup();
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
  MozLoopServiceInternal.fxAOAuthProfile = {email: "test@example.com", uid: "abcd1234"};
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
  MozLoopServiceInternal.fxAOAuthTokenData = {token_type:"bearer",access_token:"1bad3e44b12f77a88fe09f016f6a37c42e40f974bc7a8b432bb0d2f0e37e1752",scope:"profile"};
  MozLoopServiceInternal.fxAOAuthProfile = {email: "test@example.com", uid: "abcd1234"};
  yield MozLoopServiceInternal.notifyStatusChanged("login");
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "active", "Check button is in active state");
  yield loadLoopPanel();
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "", "Check button is in default state after opening panel");
  let loopPanel = document.getElementById("loop-notification-panel");
  loopPanel.hidePopup();
  MozLoopServiceInternal.fxAOAuthTokenData = null;
  MozLoopServiceInternal.notifyStatusChanged();
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "", "Check button is in default state");
});

