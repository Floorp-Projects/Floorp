/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  TelemetryTestUtils: "resource://testing-common/TelemetryTestUtils.sys.mjs",
  sinon: "resource://testing-common/Sinon.sys.mjs",
});

// Helpers for testing telemetry events.

// Tests can change the category to filter for different events.
var gTelemetryCategory = "upgrade_dialog";

function AssertEvents(message, ...events) {
  info(`Checking telemetry events: ${message}`);
  TelemetryTestUtils.assertEvents(
    events.map(event => [gTelemetryCategory, ...event]),
    { category: gTelemetryCategory }
  );
}

const BROWSER_GLUE =
  Cc["@mozilla.org/browser/browserglue;1"].getService().wrappedJSObject;

// Helpers for mocking various shell states.

let didMockShell = false;
function mockShell(overrides = {}) {
  if (!didMockShell) {
    sinon.stub(window, "getShellService");
    registerCleanupFunction(() => {
      getShellService.restore();
    });
    didMockShell = true;
  }

  const sharedPinStub = sinon.stub().resolves(undefined);
  let mock = {
    canPin: false,
    isDefault: false,
    isPinned: false,

    async checkPinCurrentAppToTaskbarAsync(privateBrowsing = false) {
      if (!this.canPin) {
        throw Error;
      }
    },
    get isAppInDock() {
      return this.isPinned;
    },
    isCurrentAppPinnedToTaskbarAsync(privateBrowsing = false) {
      return Promise.resolve(this.isPinned);
    },
    isDefaultBrowser() {
      return this.isDefault;
    },
    get macDockSupport() {
      return this;
    },
    // eslint-disable-next-line mozilla/use-chromeutils-generateqi
    QueryInterface() {
      return this;
    },
    get shellService() {
      return this;
    },

    ensureAppIsPinnedToDock: sharedPinStub,
    pinCurrentAppToTaskbarAsync: sharedPinStub,
    setAsDefault: sinon.stub(),
    ...overrides,
  };

  // Prefer the mocked implementation and fall back to the original version,
  // which can call back into the mocked version (via this.shellService).
  mock = new Proxy(mock, {
    get(target, prop) {
      return (prop in target ? target : ShellService)[prop];
    },
    set(target, prop, val) {
      (prop in target ? target : ShellService)[prop] = val;
      return true;
    },
  });

  getShellService.returns(mock);
  return mock;
}
