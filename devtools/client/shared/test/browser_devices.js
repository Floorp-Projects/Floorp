/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  getDevices,
  getDeviceString,
  addDevice,
} = require("devtools/client/shared/devices");

add_task(async function() {
  let devices = await getDevices();

  ok(devices.TYPES.length > 0, `Found ${devices.TYPES.length} device types.`);

  for (const type of devices.TYPES) {
    const string = getDeviceString(type);
    ok(
      typeof string === "string" &&
        string.length > 0 &&
        string != `device.${type}`,
      `Able to localize "${type}": "${string}"`
    );

    ok(
      devices[type].length > 0,
      `Found ${devices[type].length} ${type} devices`
    );
  }

  const type1 = devices.TYPES[0];
  const type1DeviceCount = devices[type1].length;

  const device1 = {
    name: "SquarePhone",
    width: 320,
    height: 320,
    pixelRatio: 2,
    userAgent: "Mozilla/5.0 (Mobile; rv:42.0)",
    touch: true,
    firefoxOS: true,
  };
  addDevice(device1, devices.TYPES[0]);
  devices = await getDevices();

  is(
    devices[type1].length,
    type1DeviceCount + 1,
    `Added new device of type "${type1}".`
  );
  ok(
    devices[type1].find(d => d.name === device1.name),
    "Found the new device."
  );

  const type2 = "appliances";
  const device2 = {
    name: "Mr Freezer",
    width: 800,
    height: 600,
    pixelRatio: 5,
    userAgent: "Mozilla/5.0 (Appliance; rv:42.0)",
    touch: true,
    firefoxOS: true,
  };

  const typeCount = devices.TYPES.length;
  addDevice(device2, type2);
  devices = await getDevices();

  is(devices.TYPES.length, typeCount + 1, `Added device type "${type2}".`);
  is(devices[type2].length, 1, `Added new "${type2}" device`);
});
