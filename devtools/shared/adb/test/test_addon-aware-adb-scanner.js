/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const { AddonAwareADBScanner } = require("devtools/shared/adb/addon-aware-adb-scanner");

/**
 * For the scanner mock, we create an object with spies for each of the public methods
 * used by the AddonAwareADBScanner, and the ability to emit events.
 */
function prepareMockScanner() {
  const mockScanner = {
    enable: sinon.spy(),
    disable: sinon.spy(),
    scan: sinon.spy(),
    listRuntimes: sinon.spy(),
  };
  EventEmitter.decorate(mockScanner);
  return mockScanner;
}

/**
 * For the addon mock, we simply need an object that is able to emit events and has a
 * status.
 */
function prepareMockAddon() {
  const mockAddon = {
    status: "unknown",
  };
  EventEmitter.decorate(mockAddon);
  return mockAddon;
}

/**
 * Prepare all mocks needed for the scanner tests.
 */
function prepareMocks() {
  const mockScanner = prepareMockScanner();
  const mockAddon = prepareMockAddon();
  const addonAwareAdbScanner = new AddonAwareADBScanner(mockScanner, mockAddon);
  return { addonAwareAdbScanner, mockAddon, mockScanner };
}

/**
 * This test covers basic usage of enable() on the AddonAwareADBScanner, and checks the
 * different behaviors based on the addon status.
 */
add_task(async function testCallingEnable() {
  const { mockScanner, mockAddon, addonAwareAdbScanner } = prepareMocks();

  // Check that enable() is not called if the addon is uninstalled
  mockAddon.status = "uninstalled";
  addonAwareAdbScanner.enable();
  ok(mockScanner.enable.notCalled, "enable() was not called");
  mockScanner.enable.reset();

  // Check that enable() is called if the addon is installed
  mockAddon.status = "installed";
  addonAwareAdbScanner.enable();
  ok(mockScanner.enable.called, "enable() was called");
  mockScanner.enable.reset();
});

/**
 * This test checks that enable()/disable() methods from the internal ADBScanner are
 * called when the addon is installed or uninstalled.
 */
add_task(async function testUpdatingAddonEnablesDisablesScanner() {
  const { mockScanner, mockAddon, addonAwareAdbScanner } = prepareMocks();

  // Enable the addon aware scanner
  addonAwareAdbScanner.enable();
  ok(mockScanner.enable.notCalled, "enable() was not called initially");

  // Check that enable() is called automatically when the addon is installed
  mockAddon.status = "installed";
  mockAddon.emit("update");
  ok(mockScanner.enable.called, "enable() was called when installing the addon");
  ok(mockScanner.disable.notCalled, "disable() was not called when installing the addon");
  mockScanner.enable.reset();
  mockScanner.disable.reset();

  // Check that disabled() is called automatically when the addon is uninstalled
  mockAddon.status = "uninstalled";
  mockAddon.emit("update");
  ok(mockScanner.enable.notCalled, "enable() was not called when uninstalling the addon");
  ok(mockScanner.disable.called, "disable() was called when uninstalling the addon");
  mockScanner.enable.reset();
  mockScanner.disable.reset();

  // Check that enable() is called again when the addon is reinstalled
  mockAddon.status = "installed";
  mockAddon.emit("update");
  ok(mockScanner.enable.called, "enable() was called when installing the addon");
  ok(mockScanner.disable.notCalled, "disable() was not called when installing the addon");
  mockScanner.enable.reset();
  mockScanner.disable.reset();
});

/**
 * This test checks that disable() is forwarded from the AddonAwareADBScanner to the real
 * scanner even if the addon is uninstalled. We might miss the addon uninstall
 * notification, so it is safer to always proceed with disabling.
 */
add_task(async function testScannerIsDisabledWhenMissingAddonUpdate() {
  const { mockScanner, mockAddon, addonAwareAdbScanner } = prepareMocks();

  // Enable the addon aware scanner
  mockAddon.status = "installed";
  addonAwareAdbScanner.enable();
  ok(mockScanner.enable.called, "enable() was called initially");
  mockScanner.enable.reset();

  // Uninstall the addon without firing any event
  mockAddon.status = "uninstalled";

  // Programmatically call disable, check that the scanner's disable is called even though
  // the addon was uninstalled.
  addonAwareAdbScanner.disable();
  ok(mockScanner.disable.called, "disable() was called when uninstalling the addon");
  mockScanner.disable.reset();
});

/**
 * This test checks that when the AddonAwareADBScanner is disabled, then enable/disable
 * are not called on the inner scanner when the addon status changes.
 */
add_task(async function testInnerEnableIsNotCalledIfNotStarted() {
  const { mockScanner, mockAddon, addonAwareAdbScanner } = prepareMocks();

  // Check that enable() is not called on the inner scanner when the addon is installed
  // if the AddonAwareADBScanner was not enabled
  mockAddon.status = "installed";
  mockAddon.emit("update");
  ok(mockScanner.enable.notCalled, "enable() was not called");

  // Same for disable() and "uninstall"
  mockAddon.status = "uninstalled";
  mockAddon.emit("update");
  ok(mockScanner.disable.notCalled, "disable() was not called");

  // Enable the addon aware scanner
  addonAwareAdbScanner.enable();
  ok(mockScanner.enable.notCalled, "enable() was not called");
  ok(mockScanner.disable.notCalled, "disable() was not called");
});

/**
 * This test checks that when the AddonAwareADBScanner is disabled, installing the addon
 * no longer enables the internal ADBScanner.
 */
add_task(async function testEnableIsNoLongerCalledAfterDisabling() {
  const { mockScanner, mockAddon, addonAwareAdbScanner } = prepareMocks();

  // Start with the addon installed
  mockAddon.status = "installed";
  addonAwareAdbScanner.enable();
  ok(mockScanner.enable.called, "enable() was called since addon was already installed");
  mockScanner.enable.reset();

  // Here we call enable again to check that we will not add too many events.
  // A single call to disable() should stop all listeners, even if we called enable()
  // several times.
  addonAwareAdbScanner.enable();
  ok(mockScanner.enable.called, "enable() was called again");
  mockScanner.enable.reset();

  // Disable the scanner
  addonAwareAdbScanner.disable();
  ok(mockScanner.disable.called, "disable() was called");
  mockScanner.disable.reset();

  // Emit an addon update event
  mockAddon.emit("update");
  ok(mockScanner.enable.notCalled,
    "enable() is not called since the main scanner is disabled");
});

/**
 * Basic check that the "runtime-list-updated" event is forwarded.
 */
add_task(async function testListUpdatedEventForwarding() {
  const { mockScanner, addonAwareAdbScanner } = prepareMocks();

  const spy = sinon.spy();
  addonAwareAdbScanner.on("runtime-list-updated", spy);
  mockScanner.emit("runtime-list-updated");
  ok(spy.called, "The runtime-list-updated event was forwarded from ADBScanner");
  addonAwareAdbScanner.off("runtime-list-updated", spy);
});

/**
 * Basic check that calls to scan() are forwarded.
 */
add_task(async function testScanCallForwarding() {
  const { mockScanner, addonAwareAdbScanner } = prepareMocks();

  ok(mockScanner.scan.notCalled, "ADBScanner scan() is not called initially");

  addonAwareAdbScanner.scan();
  mockScanner.emit("runtime-list-updated");
  ok(mockScanner.scan.called, "ADBScanner scan() was called");
  mockScanner.scan.reset();
});

/**
 * Basic check that calls to scan() are forwarded.
 */
add_task(async function testListRuntimesCallForwarding() {
  const { mockScanner, addonAwareAdbScanner } = prepareMocks();

  ok(mockScanner.listRuntimes.notCalled,
    "ADBScanner listRuntimes() is not called initially");

  addonAwareAdbScanner.listRuntimes();
  mockScanner.emit("runtime-list-updated");
  ok(mockScanner.listRuntimes.called, "ADBScanner listRuntimes() was called");
  mockScanner.scan.reset();
});
