"use strict";

/**
 * This test ensures that we correctly clean up the session state before
 * writing to disk a last time on shutdown. For now it only tests that each
 * tab's shistory is capped to a maximum number of preceding and succeeding
 * entries.
 */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { SessionWorker } = ChromeUtils.import(
  "resource:///modules/sessionstore/SessionWorker.jsm"
);

const profd = do_get_profile();
const { SessionFile } = ChromeUtils.import(
  "resource:///modules/sessionstore/SessionFile.jsm"
);
const { Paths } = SessionFile;

const { File } = OS;

const MAX_ENTRIES = 9;
const URL = "http://example.com/#";

// We need a XULAppInfo to initialize SessionFile
const { updateAppInfo } = ChromeUtils.import(
  "resource://testing-common/AppInfo.jsm"
);
updateAppInfo({
  name: "SessionRestoreTest",
  ID: "{230de50e-4cd1-11dc-8314-0800200c9a66}",
  version: "1",
  platformVersion: "",
});

var gSourceHandle;

async function prepareWithLimit(back, fwd) {
  await SessionFile.wipe();

  if (!gSourceHandle) {
    gSourceHandle = do_get_file("data/sessionstore_valid.js");
  }
  gSourceHandle.copyTo(profd, "sessionstore.js");
  await writeCompressedFile(Paths.clean.replace("jsonlz4", "js"), Paths.clean);

  Services.prefs.setIntPref("browser.sessionstore.max_serialize_back", back);
  Services.prefs.setIntPref("browser.sessionstore.max_serialize_forward", fwd);

  // Finish SessionFile initialization.
  await SessionFile.read();
}

add_task(async function setup() {
  await SessionFile.read();

  // Reset prefs on cleanup.
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.sessionstore.max_serialize_back");
    Services.prefs.clearUserPref("browser.sessionstore.max_serialize_forward");
  });
});

function createSessionState(index) {
  // Generate the tab state entries and set the one-based
  // tab-state index to the middle session history entry.
  let tabState = { entries: [], index };
  for (let i = 0; i < MAX_ENTRIES; i++) {
    tabState.entries.push({ url: URL + i });
  }

  return { windows: [{ tabs: [tabState] }] };
}

async function writeAndParse(state, path, options = {}) {
  await SessionWorker.post("write", [state, options]);
  return JSON.parse(
    await File.read(path, { encoding: "utf-8", compression: "lz4" })
  );
}

add_task(async function test_shistory_cap_none() {
  let state = createSessionState(5);

  // Don't limit the number of shistory entries.
  await prepareWithLimit(-1, -1);

  // Check that no caps are applied.
  let diskState = await writeAndParse(state, Paths.clean, {
    isFinalWrite: true,
  });
  Assert.deepEqual(state, diskState, "no cap applied");
});

add_task(async function test_shistory_cap_middle() {
  let state = createSessionState(5);
  await prepareWithLimit(2, 3);

  // Cap is only applied on clean shutdown.
  let diskState = await writeAndParse(state, Paths.recovery);
  Assert.deepEqual(state, diskState, "no cap applied");

  // Check that the right number of shistory entries was discarded
  // and the shistory index updated accordingly.
  diskState = await writeAndParse(state, Paths.clean, { isFinalWrite: true });
  let tabState = state.windows[0].tabs[0];
  tabState.entries = tabState.entries.slice(2, 8);
  tabState.index = 3;
  Assert.deepEqual(state, diskState, "cap applied");
});

add_task(async function test_shistory_cap_lower_bound() {
  let state = createSessionState(1);
  await prepareWithLimit(5, 5);

  // Cap is only applied on clean shutdown.
  let diskState = await writeAndParse(state, Paths.recovery);
  Assert.deepEqual(state, diskState, "no cap applied");

  // Check that the right number of shistory entries was discarded.
  diskState = await writeAndParse(state, Paths.clean, { isFinalWrite: true });
  let tabState = state.windows[0].tabs[0];
  tabState.entries = tabState.entries.slice(0, 6);
  Assert.deepEqual(state, diskState, "cap applied");
});

add_task(async function test_shistory_cap_upper_bound() {
  let state = createSessionState(MAX_ENTRIES);
  await prepareWithLimit(5, 5);

  // Cap is only applied on clean shutdown.
  let diskState = await writeAndParse(state, Paths.recovery);
  Assert.deepEqual(state, diskState, "no cap applied");

  // Check that the right number of shistory entries was discarded
  // and the shistory index updated accordingly.
  diskState = await writeAndParse(state, Paths.clean, { isFinalWrite: true });
  let tabState = state.windows[0].tabs[0];
  tabState.entries = tabState.entries.slice(3);
  tabState.index = 6;
  Assert.deepEqual(state, diskState, "cap applied");
});

add_task(async function cleanup() {
  await SessionFile.wipe();
  await SessionFile.read();
});
