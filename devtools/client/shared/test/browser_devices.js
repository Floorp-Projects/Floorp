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

  let types = [...devices.keys()];
  ok(!!types.length, `Found ${types.length} device types.`);

  for (const type of types) {
    const string = getDeviceString(type);
    ok(
      typeof string === "string" &&
        !!string.length &&
        string != `device.${type}`,
      `Able to localize "${type}": "${string}"`
    );

    ok(
      !!devices.get(type).length,
      `Found ${devices.get(type).length} ${type} devices`
    );
  }

  const type1 = types[0];
  const type1DeviceCount = devices.get(type1).length;

  const device1 = {
    name: "SquarePhone",
    width: 320,
    height: 320,
    pixelRatio: 2,
    userAgent: "Mozilla/5.0 (Mobile; rv:42.0)",
    touch: true,
    firefoxOS: true,
  };
  addDevice(device1, types[0]);
  devices = await getDevices();

  is(
    devices.get(type1).length,
    type1DeviceCount + 1,
    `Added new device of type "${type1}".`
  );
  ok(
    devices.get(type1).find(d => d.name === device1.name),
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

  const typeCount = types.length;
  addDevice(device2, type2);
  devices = await getDevices();
  types = [...devices.keys()];

  is(types.length, typeCount + 1, `Added device type "${type2}".`);
  is(devices.get(type2).length, 1, `Added new "${type2}" device`);
});
