/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

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
