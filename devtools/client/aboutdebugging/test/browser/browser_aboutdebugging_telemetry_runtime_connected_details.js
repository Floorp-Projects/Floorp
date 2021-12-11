/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-telemetry.js */
Services.scriptloader.loadSubScript(
  CHROME_URL_ROOT + "helper-telemetry.js",
  this
);

const REMOTE_RUNTIME_ID = "remote-runtime";
const REMOTE_RUNTIME = "Remote Runtime";
const REMOTE_DEVICE = "Remote Device";

const REMOTE_VERSION = "12.0a1";
const REMOTE_OS = "SOME_OS";

/**
 * Runtime connected events will log additional extras about the runtime connection that
 * was established.
 */
add_task(async function() {
  const mocks = new Mocks();

  const usbClient = mocks.createUSBRuntime(REMOTE_RUNTIME_ID, {
    deviceName: REMOTE_DEVICE,
    name: REMOTE_RUNTIME,
    shortName: REMOTE_RUNTIME,
  });
  usbClient.getDeviceDescription = () => {
    return {
      os: REMOTE_OS,
      version: REMOTE_VERSION,
    };
  };

  const { document, tab } = await openAboutDebugging();

  mocks.emitUSBUpdate();
  await connectToRuntime(REMOTE_DEVICE, document);
  const evts = readAboutDebuggingEvents().filter(
    e => e.method === "runtime_connected"
  );

  is(
    evts.length,
    1,
    "runtime_connected event logged when connecting to remote runtime"
  );
  const {
    connection_type,
    device_name,
    runtime_name,
    runtime_os,
    runtime_version,
  } = evts[0].extras;
  is(connection_type, "usb", "Expected value for `connection_type` extra");
  is(device_name, REMOTE_DEVICE, "Expected value for `device_name` extra");
  is(runtime_name, REMOTE_RUNTIME, "Expected value for `runtime_name` extra");
  is(runtime_os, REMOTE_OS, "Expected value for `runtime_os` extra");
  is(
    runtime_version,
    REMOTE_VERSION,
    "Expected value for `runtime_version` extra"
  );

  await removeTab(tab);
});
