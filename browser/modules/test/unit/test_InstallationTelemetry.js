/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

const { BrowserUsageTelemetry } = ChromeUtils.import(
  "resource:///modules/BrowserUsageTelemetry.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "FileUtils",
  "resource://gre/modules/FileUtils.jsm"
);

const TIMESTAMP_PREF = "app.installation.timestamp";

function encodeUtf16(str) {
  const buf = new ArrayBuffer(str.length * 2);
  const utf16 = new Uint16Array(buf);
  for (let i = 0; i < str.length; i++) {
    utf16[i] = str.charCodeAt(i);
  }
  return new Uint8Array(buf);
}

// Returns Promise
function writeJsonUtf16(file, obj) {
  const str = JSON.stringify(obj);
  return IOUtils.write(file.path, encodeUtf16(str));
}

async function runReport(
  dataFilePath,
  installType,
  { clearTS, setTS, assertRejects, expectExtra, expectTS }
) {
  // Setup timestamp
  if (clearTS) {
    Services.prefs.clearUserPref(TIMESTAMP_PREF);
  }
  if (typeof setTS == "string") {
    Services.prefs.setStringPref(TIMESTAMP_PREF, setTS);
  }

  // Init events
  Services.telemetry.clearEvents();

  // Exercise reportInstallationTelemetry
  if (typeof assertRejects != "undefined") {
    await Assert.rejects(
      BrowserUsageTelemetry.reportInstallationTelemetry(dataFilePath),
      assertRejects
    );
  } else {
    await BrowserUsageTelemetry.reportInstallationTelemetry(dataFilePath);
  }

  // Check events
  TelemetryTestUtils.assertEvents(
    expectExtra
      ? [{ object: installType, value: null, extra: expectExtra }]
      : [],
    { category: "installation", method: "first_seen" }
  );

  // Check timestamp
  if (typeof expectTS == "string") {
    Assert.equal(expectTS, Services.prefs.getStringPref(TIMESTAMP_PREF));
  }
}

add_task(async function testInstallationTelemetry() {
  let dataFile = FileUtils.getFile("TmpD", [
    "installation-telemetry-test-data" + Math.random() + ".json",
  ]);
  dataFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);

  registerCleanupFunction(async () => {
    try {
      await IOUtils.remove(dataFile.path);
    } catch (ex) {
      // Ignore remove failure, file may not exist by now
    }

    Services.prefs.clearUserPref(TIMESTAMP_PREF);
  });

  // Test with normal stub data
  let stubData = {
    version: "99.0abc",
    build_id: "123",
    installer_type: "stub",
    admin_user: true,
    install_existed: false,
    profdir_existed: false,
    install_timestamp: "0",
  };
  let stubExtra = {
    version: "99.0abc",
    build_id: "123",
    admin_user: "true",
    install_existed: "false",
    profdir_existed: "false",
  };

  await writeJsonUtf16(dataFile, stubData);
  await runReport(dataFile, "stub", {
    clearTS: true,
    expectExtra: stubExtra,
    expectTS: "0",
  });

  // Check that it doesn't generate another event when the timestamp is unchanged
  await runReport(dataFile, "stub", { expectTS: "0" });

  // New timestamp
  stubData.install_timestamp = "1";
  await writeJsonUtf16(dataFile, stubData);
  await runReport(dataFile, "stub", { expectExtra: stubExtra, expectTS: "1" });

  // Test with normal full data
  let fullData = {
    version: "99.0abc",
    build_id: "123",
    installer_type: "full",
    admin_user: false,
    install_existed: true,
    profdir_existed: true,
    silent: false,
    from_msi: false,
    default_path: true,

    install_timestamp: "1",
  };
  let fullExtra = {
    version: "99.0abc",
    build_id: "123",
    admin_user: "false",
    install_existed: "true",
    profdir_existed: "true",
    silent: "false",
    from_msi: "false",
    default_path: "true",
  };

  await writeJsonUtf16(dataFile, fullData);
  await runReport(dataFile, "full", {
    clearTS: true,
    expectExtra: fullExtra,
    expectTS: "1",
  });

  // Check that it doesn't generate another event when the timestamp is unchanged
  await runReport(dataFile, "full", { expectTS: "1" });

  // New timestamp
  fullData.install_timestamp = "2";
  await writeJsonUtf16(dataFile, fullData);
  await runReport(dataFile, "full", { expectExtra: fullExtra, expectTS: "2" });

  // Missing field
  delete fullData.install_existed;
  fullData.install_timestamp = "3";
  await writeJsonUtf16(dataFile, fullData);
  await runReport(dataFile, "full", { assertRejects: /install_existed/ });

  // Malformed JSON
  await IOUtils.write(dataFile.path, encodeUtf16("hello"));
  await runReport(dataFile, "stub", { assertRejects: /unexpected character/ });

  // Missing file, should return with no exception
  await IOUtils.remove(dataFile.path);
  await runReport(dataFile, "stub", { setTS: "3", expectTS: "3" });
});
