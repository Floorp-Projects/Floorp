/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const COMPATIBLE_RUNTIME = "Compatible Runtime";
const COMPATIBLE_DEVICE = "Compatible Device";
const OLD_RUNTIME = "Old Runtime";
const OLD_DEVICE = "Old Device";
const DEBUGGER_67_RUNTIME = "Bad Runtime Debugger 67";
const DEBUGGER_67_DEVICE = "Bad Device Debugger 67";
const RECENT_RUNTIME = "Recent Runtime";
const RECENT_DEVICE = "Recent Device";

add_task(async function() {
  const {
    COMPATIBILITY_STATUS,
  } = require("devtools/client/shared/remote-debugging/version-checker");
  const {
    COMPATIBLE,
    TOO_OLD,
    TOO_OLD_67_DEBUGGER,
    TOO_RECENT,
  } = COMPATIBILITY_STATUS;

  info("Create several mocked runtimes, with different compatibility reports");
  const mocks = new Mocks();
  createRuntimeWithReport(
    mocks,
    COMPATIBLE_RUNTIME,
    COMPATIBLE_DEVICE,
    COMPATIBLE
  );
  createRuntimeWithReport(mocks, OLD_RUNTIME, OLD_DEVICE, TOO_OLD);
  createRuntimeWithReport(mocks, RECENT_RUNTIME, RECENT_DEVICE, TOO_RECENT);
  createRuntimeWithReport(
    mocks,
    DEBUGGER_67_RUNTIME,
    DEBUGGER_67_DEVICE,
    TOO_OLD_67_DEBUGGER
  );

  const { document, tab } = await openAboutDebugging();
  mocks.emitUSBUpdate();

  info("Connect to all runtimes");
  await connectToRuntime(COMPATIBLE_DEVICE, document);
  await connectToRuntime(OLD_DEVICE, document);
  await connectToRuntime(RECENT_DEVICE, document);
  await connectToRuntime(DEBUGGER_67_DEVICE, document);

  info("Select the compatible runtime and check that no warning is displayed");
  await selectRuntime(COMPATIBLE_DEVICE, COMPATIBLE_RUNTIME, document);
  ok(
    !document.querySelector(".qa-compatibility-warning"),
    "Compatibility warning is not displayed"
  );

  info(
    "Select the old runtime and check that the too-old warning is displayed"
  );
  await selectRuntime(OLD_DEVICE, OLD_RUNTIME, document);
  ok(
    document.querySelector(".qa-compatibility-warning-too-old"),
    "Expected compatibility warning is displayed (too-old)"
  );

  info(
    "Select the recent runtime and check that the too-recent warning is displayed"
  );
  await selectRuntime(RECENT_DEVICE, RECENT_RUNTIME, document);
  ok(
    document.querySelector(".qa-compatibility-warning-too-recent"),
    "Expected compatibility warning is displayed (too-recent)"
  );

  info(
    "Select the runtime incompatible with Fx 67 " +
      "and check that the debugger 67 warning is displayed"
  );
  await selectRuntime(DEBUGGER_67_DEVICE, DEBUGGER_67_RUNTIME, document);
  ok(document.querySelector(".qa-compatibility-warning-too-old-67-debugger"));

  await removeTab(tab);
});

function createRuntimeWithReport(mocks, name, deviceName, status) {
  const runtimeId = [name, deviceName].join("-");
  const compatibleUsbClient = mocks.createUSBRuntime(runtimeId, {
    deviceName,
    name,
  });
  const report = { status };
  compatibleUsbClient.checkVersionCompatibility = () => report;
}
