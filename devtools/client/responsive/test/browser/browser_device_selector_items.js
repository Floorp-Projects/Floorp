/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the device selector button and the menu items.

const MenuItem = require("devtools/client/shared/components/menu/MenuItem");

const FIREFOX_ICON =
  'url("chrome://devtools/skin/images/browsers/firefox.svg")';
const DUMMY_ICON = `url("${MenuItem.DUMMY_ICON}")`;

const FIREFOX_DEVICE = {
  name: "Device of Firefox user-agent",
  userAgent: "Mozilla/5.0 (Mobile; rv:39.0) Gecko/39.0 Firefox/39.0",
  width: 320,
  height: 570,
  pixelRatio: 5.5,
  touch: true,
  firefoxOS: true,
  os: "custom",
  featured: true,
};

const TEST_DEVICES = [
  {
    name: FIREFOX_DEVICE.name,
    hasIcon: true,
  },
  {
    name: "Laptop with MDPI screen",
    hasIcon: false,
  },
];

addDeviceForTest(FIREFOX_DEVICE);

// Add the laptop to the device list
const {
  updatePreferredDevices,
} = require("devtools/client/responsive/actions/devices");
updatePreferredDevices({
  added: ["Laptop with MDPI screen"],
  removed: [],
});

addRDMTask(
  URL_ROOT,
  async function({ ui }) {
    const deviceSelector = ui.toolWindow.document.getElementById(
      "device-selector"
    );

    for (const testDevice of TEST_DEVICES) {
      info(`Check "${name}" device`);
      await testMenuItems(ui.toolWindow, deviceSelector, menuItems => {
        const menuItem = findMenuItem(menuItems, testDevice.name);
        ok(menuItem, "The menu item is on the list");
        const label = menuItem.querySelector(".iconic > .label");
        const backgroundImage = ui.toolWindow.getComputedStyle(
          label,
          "::before"
        ).backgroundImage;
        const icon = testDevice.hasIcon ? FIREFOX_ICON : DUMMY_ICON;
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
  },
  { waitForDeviceList: true }
);
