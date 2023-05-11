/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

const { UIState } = ChromeUtils.importESModule(
  "resource://services-sync/UIState.sys.mjs"
);

var gTestTab;
var gContentAPI;

add_task(setup_UITourTest);

add_UITour_task(async function test_no_user() {
  const sandbox = sinon.createSandbox();
  sandbox.stub(fxAccounts, "getSignedInUser").returns(null);
  let result = await getConfigurationPromise("fxa");
  Assert.deepEqual(result, { setup: false });
  sandbox.restore();
});

add_UITour_task(async function test_no_sync_no_devices() {
  const sandbox = sinon.createSandbox();
  sandbox
    .stub(fxAccounts, "getSignedInUser")
    .returns({ email: "foo@example.com" });
  sandbox.stub(fxAccounts.device, "recentDeviceList").get(() => {
    return [
      {
        id: 1,
        name: "This Device",
        isCurrentDevice: true,
        type: "desktop",
      },
    ];
  });
  sandbox.stub(fxAccounts, "listAttachedOAuthClients").resolves([]);
  sandbox.stub(fxAccounts, "hasLocalSession").resolves(true);

  let result = await getConfigurationPromise("fxaConnections");
  Assert.deepEqual(result, {
    setup: true,
    numOtherDevices: 0,
    numDevicesByType: {},
    accountServices: {},
  });
  sandbox.restore();
});

add_UITour_task(async function test_no_sync_many_devices() {
  const sandbox = sinon.createSandbox();
  sandbox
    .stub(fxAccounts, "getSignedInUser")
    .returns({ email: "foo@example.com" });
  sandbox.stub(fxAccounts.device, "recentDeviceList").get(() => {
    return [
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
      {
        id: 3,
        name: "My phone",
        type: "phone",
      },
      {
        id: 4,
        name: "Who knows?",
      },
      {
        id: 5,
        name: "Another desktop",
        type: "desktop",
      },
      {
        id: 6,
        name: "Yet Another desktop",
        type: "desktop",
      },
    ];
  });
  sandbox.stub(fxAccounts, "listAttachedOAuthClients").resolves([]);
  sandbox.stub(fxAccounts, "hasLocalSession").resolves(true);

  let result = await getConfigurationPromise("fxaConnections");
  Assert.deepEqual(result, {
    setup: true,
    accountServices: {},
    numOtherDevices: 5,
    numDevicesByType: {
      desktop: 2,
      mobile: 1,
      phone: 1,
      unknown: 1,
    },
  });
  sandbox.restore();
});

add_UITour_task(async function test_fxa_connections_no_cached_devices() {
  const sandbox = sinon.createSandbox();
  sandbox
    .stub(fxAccounts, "getSignedInUser")
    .returns({ email: "foo@example.com" });
  let devicesStub = sandbox.stub(fxAccounts.device, "recentDeviceList");
  devicesStub.get(() => {
    // Sinon doesn't seem to support second `getters` returning a different
    // value, so replace the getter here.
    devicesStub.get(() => {
      return [
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
      ];
    });
    // and here we want to say "nothing is yet cached"
    return null;
  });

  sandbox.stub(fxAccounts, "listAttachedOAuthClients").resolves([]);
  sandbox.stub(fxAccounts, "hasLocalSession").resolves(true);
  let rdlStub = sandbox.stub(fxAccounts.device, "refreshDeviceList").resolves();

  let result = await getConfigurationPromise("fxaConnections");
  Assert.deepEqual(result, {
    setup: true,
    accountServices: {},
    numOtherDevices: 1,
    numDevicesByType: {
      mobile: 1,
    },
  });
  Assert.ok(rdlStub.called);
  sandbox.restore();
});

add_UITour_task(async function test_account_connections() {
  const sandbox = sinon.createSandbox();
  sandbox
    .stub(fxAccounts, "getSignedInUser")
    .returns({ email: "foo@example.com" });
  sandbox.stub(fxAccounts.device, "recentDeviceList").get(() => []);
  sandbox.stub(fxAccounts, "listAttachedOAuthClients").resolves([
    {
      id: "802d56ef2a9af9fa",
      lastAccessedDaysAgo: 2,
    },
    {
      id: "1f30e32975ae5112",
      lastAccessedDaysAgo: 10,
    },
    {
      id: null,
      name: "Some browser",
      lastAccessedDaysAgo: 10,
    },
    {
      id: "null-last-accessed",
      lastAccessedDaysAgo: null,
    },
  ]);
  Assert.deepEqual(await getConfigurationPromise("fxaConnections"), {
    setup: true,
    numOtherDevices: 0,
    numDevicesByType: {},
    accountServices: {
      "802d56ef2a9af9fa": {
        id: "802d56ef2a9af9fa",
        lastAccessedWeeksAgo: 0,
      },
      "1f30e32975ae5112": {
        id: "1f30e32975ae5112",
        lastAccessedWeeksAgo: 1,
      },
      "null-last-accessed": {
        id: "null-last-accessed",
        lastAccessedWeeksAgo: null,
      },
    },
  });
  sandbox.restore();
});

add_UITour_task(async function test_sync() {
  const sandbox = sinon.createSandbox();
  sandbox
    .stub(fxAccounts, "getSignedInUser")
    .returns({ email: "foo@example.com" });
  sandbox.stub(fxAccounts.device, "recentDeviceList").get(() => []);
  sandbox.stub(fxAccounts, "listAttachedOAuthClients").resolves([]);
  sandbox.stub(fxAccounts, "hasLocalSession").resolves(true);
  Services.prefs.setCharPref("services.sync.username", "tests@mozilla.org");
  Services.prefs.setIntPref("services.sync.clients.devices.desktop", 4);
  Services.prefs.setIntPref("services.sync.clients.devices.mobile", 5);
  Services.prefs.setIntPref("services.sync.numClients", 9);

  Assert.deepEqual(await getConfigurationPromise("fxa"), {
    setup: true,
    accountStateOK: true,
    browserServices: {
      sync: {
        setup: true,
        mobileDevices: 5,
        desktopDevices: 4,
        totalDevices: 9,
      },
    },
  });
  Services.prefs.clearUserPref("services.sync.username");
  Services.prefs.clearUserPref("services.sync.clients.devices.desktop");
  Services.prefs.clearUserPref("services.sync.clients.devices.mobile");
  Services.prefs.clearUserPref("services.sync.numClients");
  sandbox.restore();
});

add_UITour_task(async function test_fxa_fails() {
  const sandbox = sinon.createSandbox();
  sandbox.stub(fxAccounts, "getSignedInUser").throws();
  let result = await getConfigurationPromise("fxa");
  Assert.deepEqual(result, {});
  sandbox.restore();
});

/**
 * Tests that a UITour page can get notifications on FxA sign-in state
 * changes.
 */
add_UITour_task(async function test_fxa_signedin_state_change() {
  const sandbox = sinon.createSandbox();
  registerCleanupFunction(() => {
    sandbox.restore();
  });

  let fxaConfig = await getConfigurationPromise("fxa");
  Assert.ok(!fxaConfig.setup, "FxA should not yet be set up.");

  // A helper function that waits for the state change event to fire
  // in content, and returns a Promise that resolves to the status
  // parameter on the event detail.
  let waitForSignedInStateChange = () => {
    return SpecialPowers.spawn(gTestTab.linkedBrowser, [], async () => {
      let event = await ContentTaskUtils.waitForEvent(
        content.document,
        "mozUITourNotification",
        false,
        e => {
          return e.detail.event === "FxA:SignedInStateChange";
        },
        true
      );
      return event.detail.params.status;
    });
  };

  // We'll first test the STATUS_SIGNED_IN status.

  let stateChangePromise = waitForSignedInStateChange();

  // Per bug 1743857, we wait for a JSWindowActor message round trip to
  // ensure that the mozUITourNotification event listener has been setup
  // in the SpecialPowers.spawn task.
  await new Promise(resolve => {
    gContentAPI.ping(resolve);
  });

  let UIStateStub = sandbox.stub(UIState, "get").returns({
    status: UIState.STATUS_SIGNED_IN,
    syncEnabled: true,
    email: "email@example.com",
  });

  Services.obs.notifyObservers(null, UIState.ON_UPDATE);

  let status = await stateChangePromise;
  Assert.equal(
    status,
    UIState.STATUS_SIGNED_IN,
    "FxA:SignedInStateChange should have notified that we'd signed in."
  );

  // We'll next test the STATUS_NOT_CONFIGURED status.

  stateChangePromise = waitForSignedInStateChange();

  // Per bug 1743857, we wait for a JSWindowActor message round trip to
  // ensure that the mozUITourNotification event listener has been setup
  // in the SpecialPowers.spawn task.
  await new Promise(resolve => {
    gContentAPI.ping(resolve);
  });

  UIStateStub.restore();
  UIStateStub = sandbox.stub(UIState, "get").returns({
    status: UIState.STATUS_NOT_CONFIGURED,
  });

  Services.obs.notifyObservers(null, UIState.ON_UPDATE);

  status = await stateChangePromise;
  Assert.equal(
    status,
    UIState.STATUS_NOT_CONFIGURED,
    "FxA:SignedInStateChange should have notified that we're not configured."
  );

  // We'll next test the STATUS_LOGIN_FAILED status.

  stateChangePromise = waitForSignedInStateChange();

  // Per bug 1743857, we wait for a JSWindowActor message round trip to
  // ensure that the mozUITourNotification event listener has been setup
  // in the SpecialPowers.spawn task.
  await new Promise(resolve => {
    gContentAPI.ping(resolve);
  });

  UIStateStub.restore();
  UIStateStub = sandbox.stub(UIState, "get").returns({
    email: "foo@example.com",
    status: UIState.STATUS_LOGIN_FAILED,
  });

  Services.obs.notifyObservers(null, UIState.ON_UPDATE);

  status = await stateChangePromise;
  Assert.equal(
    status,
    UIState.STATUS_LOGIN_FAILED,
    "FxA:SignedInStateChange should have notified that login has failed."
  );

  // We'll next test the STATUS_NOT_VERIFIED status.

  stateChangePromise = waitForSignedInStateChange();

  // Per bug 1743857, we wait for a JSWindowActor message round trip to
  // ensure that the mozUITourNotification event listener has been setup
  // in the SpecialPowers.spawn task.
  await new Promise(resolve => {
    gContentAPI.ping(resolve);
  });

  UIStateStub.restore();
  UIStateStub = sandbox.stub(UIState, "get").returns({
    email: "foo@example.com",
    status: UIState.STATUS_NOT_VERIFIED,
  });

  Services.obs.notifyObservers(null, UIState.ON_UPDATE);

  status = await stateChangePromise;
  Assert.equal(
    status,
    UIState.STATUS_NOT_VERIFIED,
    "FxA:SignedInStateChange should have notified that the login hasn't yet been verified."
  );

  sandbox.restore();
});
