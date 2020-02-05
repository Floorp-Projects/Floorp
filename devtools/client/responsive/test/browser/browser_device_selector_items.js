/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the device selector button and the menu items.

const Types = require("devtools/client/responsive/types");
const MenuItem = require("devtools/client/shared/components/menu/MenuItem");

const FIREFOX_ICON =
  'url("chrome://devtools/skin/images/browsers/firefox.svg")';
const UNKNOWN_ICON = `url("chrome://devtools/skin/${MenuItem.DUMMY_ICON}")`;

const TEST_DEVICES = [
  {
    name: "Device of Firefox user-agent",
    userAgent: "Mozilla/5.0 (Mobile; rv:39.0) Gecko/39.0 Firefox/39.0",
    width: 320,
    height: 570,
    pixelRatio: 5.5,
    touch: true,
    firefoxOS: true,
    os: "custom",
    featured: true,
    hasIcon: true,
  },
  {
    name: "Device of non user-agent",
    userAgent: "custome user agent",
    width: 320,
    height: 570,
    pixelRatio: 5.5,
    touch: true,
    firefoxOS: true,
    os: "custom",
    featured: true,
    hasIcon: false,
  },
];

addRDMTask(URL_ROOT, async function({ ui }) {
  for (const testDevice of TEST_DEVICES) {
    await addDeviceForTest(testDevice);
  }

  // Wait until the viewport has been added and the device list has been loaded
  await waitUntilState(
    ui.toolWindow.store,
    state =>
      state.viewports.length == 1 &&
      state.devices.listState == Types.loadableState.LOADED
  );

  const deviceSelector = ui.toolWindow.document.getElementById(
    "device-selector"
  );

  for (const testDevice of TEST_DEVICES) {
    info(`Check "${name}" device`);
    await testMenuItems(ui.toolWindow, deviceSelector, menuItems => {
      const menuItem = findMenuItem(menuItems, testDevice.name);
      ok(menuItem, "The menu item is on the list");
      const label = menuItem.querySelector(".iconic > .label");
      const backgroundImage = ui.toolWindow.getComputedStyle(label, "::before")
        .backgroundImage;
      const icon = testDevice.hasIcon ? FIREFOX_ICON : UNKNOWN_ICON;
      is(backgroundImage, icon, "The icon is correct");
    });

    info("Check device selector button");
    await selectDevice(ui, testDevice.name);
    const backgroundImage = ui.toolWindow.getComputedStyle(
      deviceSelector,
      "::before"
    ).backgroundImage;
    const icon = testDevice.hasIcon ? FIREFOX_ICON : "none";
    is(backgroundImage, icon, "The icon is correct");
  }
});
