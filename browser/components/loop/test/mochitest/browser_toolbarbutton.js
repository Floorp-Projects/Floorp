/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test the toolbar button states.
 */

"use strict";

const MozLoopServiceInternal = Cu.import("resource:///modules/loop/MozLoopService.jsm", {}).
                               MozLoopServiceInternal;

registerCleanupFunction(function*() {
  MozLoopService.doNotDisturb = false;
  yield MozLoopServiceInternal.clearError("testing");
});

add_task(function* test_doNotDisturb() {
  yield MozLoopService.doNotDisturb = true;
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "disabled", "Check button is in disabled state");
  yield MozLoopService.doNotDisturb = false;
  Assert.notStrictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "disabled", "Check button is not in disabled state");
});

add_task(function* test_error() {
  yield MozLoopServiceInternal.setError("testing", {});
  Assert.strictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "error", "Check button is in error state");
  yield MozLoopServiceInternal.clearError("testing");
  Assert.notStrictEqual(LoopUI.toolbarButton.node.getAttribute("state"), "error", "Check button is not in error state");
});
