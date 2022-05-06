/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { UIState } = ChromeUtils.import("resource://services-sync/UIState.jsm");
const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

function promiseSyncReady() {
  let service = Cc["@mozilla.org/weave/service;1"].getService(Ci.nsISupports)
    .wrappedJSObject;
  return service.whenLoaded();
}

function setupMocks({ fxaDevices = null, state = UIState.STATUS_SIGNED_IN }) {
  const sandbox = sinon.createSandbox();
  sandbox.stub(fxAccounts.device, "recentDeviceList").get(() => fxaDevices);
  sandbox.stub(UIState, "get").returns({
    status: state,
    syncEnabled: true,
  });
  sandbox.stub(fxAccounts.device, "refreshDeviceList").resolves(true);

  sandbox
    .stub(Weave.Service.clientsEngine, "getClientByFxaDeviceId")
    .callsFake(fxaDeviceId => {
      let target = fxaDevices.find(c => c.id == fxaDeviceId);
      return target ? target.clientRecord : null;
    });
  sandbox.stub(Weave.Service.clientsEngine, "getClientType").returns("desktop");

  return sandbox;
}

function testSetupState(browser, expected) {
  const { document } = browser.contentWindow;
  for (let [selector, shouldBeVisible] of Object.entries(
    expected.expectedVisible
  )) {
    const elem = document.querySelector(selector);
    if (shouldBeVisible) {
      ok(
        BrowserTestUtils.is_visible(elem),
        `Expected ${selector} to be visible`
      );
    } else {
      ok(BrowserTestUtils.is_hidden(elem), `Expected ${selector} to be hidden`);
    }
  }
}

add_setup(async function() {
  await promiseSyncReady();
  // gSync.init() is called in a requestIdleCallback. Force its initialization.
  gSync.init();

  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:myfirefox"
  );
  registerCleanupFunction(async function() {
    BrowserTestUtils.removeTab(tab);
  });
});

add_task(async function test_unconfigured_initial_state() {
  const browser = gBrowser.selectedBrowser;
  const sandbox = setupMocks({ state: UIState.STATUS_NOT_CONFIGURED });

  Services.obs.notifyObservers(null, UIState.ON_UPDATE);
  testSetupState(browser, {
    expectedVisible: {
      "#tabpickup-steps-view0": true,
      "#tabpickup-steps-view1": false,
      "#tabpickup-steps-view2": false,
      "#tabpickup-steps-view3": false,
    },
  });

  sandbox.restore();
});

add_task(async function test_signed_in() {
  const browser = gBrowser.selectedBrowser;
  const sandbox = setupMocks({
    state: UIState.STATUS_SIGNED_IN,
    fxaDevices: [
      {
        id: 1,
        name: "This Device",
        isCurrentDevice: true,
        type: "desktop",
      },
    ],
  });

  Services.obs.notifyObservers(null, UIState.ON_UPDATE);
  testSetupState(browser, {
    expectedVisible: {
      "#tabpickup-steps-view0": false,
      "#tabpickup-steps-view1": true,
      "#tabpickup-steps-view2": false,
      "#tabpickup-steps-view3": false,
    },
  });
  is(fxAccounts.device.recentDeviceList?.length, 1, "Just 1 device connected");

  sandbox.restore();
});

add_task(async function test_2nd_desktop_connected() {
  const browser = gBrowser.selectedBrowser;
  const sandbox = setupMocks({
    state: UIState.STATUS_SIGNED_IN,
    fxaDevices: [
      {
        id: 1,
        name: "This Device",
        isCurrentDevice: true,
        type: "desktop",
      },
      {
        id: 2,
        name: "Other Device",
        type: "desktop",
      },
    ],
  });

  Services.obs.notifyObservers(null, UIState.ON_UPDATE);
  testSetupState(browser, {
    expectedVisible: {
      "#tabpickup-steps-view0": false,
      "#tabpickup-steps-view1": true,
      "#tabpickup-steps-view2": false,
      "#tabpickup-steps-view3": false,
    },
  });
  is(fxAccounts.device.recentDeviceList?.length, 2, "2 devices connected");
  ok(
    fxAccounts.device.recentDeviceList?.every(
      device => device.type !== "mobile"
    ),
    "No connected device is type:mobile"
  );

  sandbox.restore();
});

add_task(async function test_mobile_connected() {
  const browser = gBrowser.selectedBrowser;
  const sandbox = setupMocks({
    state: UIState.STATUS_SIGNED_IN,
    fxaDevices: [
      {
        id: 1,
        name: "This Device",
        isCurrentDevice: true,
        type: "desktop",
      },
      {
        id: 2,
        name: "Other Device",
        type: "mobile",
      },
    ],
  });

  Services.obs.notifyObservers(null, UIState.ON_UPDATE);
  testSetupState(browser, {
    expectedVisible: {
      "#tabpickup-steps-view0": false,
      "#tabpickup-steps-view1": false,
      "#tabpickup-steps-view2": true,
      "#tabpickup-steps-view3": false,
    },
  });
  is(fxAccounts.device.recentDeviceList?.length, 2, "2 devices connected");
  ok(
    fxAccounts.device.recentDeviceList?.some(
      device => device.type !== "mobile"
    ),
    "A connected device is type:mobile"
  );

  sandbox.restore();
});
