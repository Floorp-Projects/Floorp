/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

const { BrowserUsageTelemetry } = ChromeUtils.import(
  "resource:///modules/BrowserUsageTelemetry.jsm"
);
const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const PROFILE_COUNT_SCALAR = "browser.engagement.profile_count";
// Largest possible uint32_t value represents an error.
const SCALAR_ERROR_VALUE = 0;

const FILE_OPEN_OPERATION = "open";
const ERROR_FILE_NOT_FOUND = "NotFoundError";
const ERROR_ACCESS_DENIED = "NotAllowedError";

// We will redirect I/O to/from the profile counter file to read/write this
// variable instead. That makes it easier for us to:
//   - avoid interference from any pre-existing file
//   - read and change the values in the file.
//   - clean up changes made to the file
// We will translate a null value stored here to a File Not Found error.
var gFakeProfileCounterFile = null;
// We will use this to check that the profile counter code doesn't try to write
// to multiple files (since this test will malfunction in that case due to
// gFakeProfileCounterFile only being setup to accommodate a single file).
var gProfileCounterFilePath = null;

// Storing a value here lets us test the behavior when we encounter an error
// reading or writing to the file. A null value means that no error will
// be simulated (other than possibly a NotFoundError).
var gNextReadExceptionReason = null;
var gNextWriteExceptionReason = null;

// Nothing will actually be stored in this directory, so it's not important that
// it be valid, but the leafname should be unique to this test in order to be
// sure of preventing name conflicts with the pref:
// `browser.engagement.profileCounted.${hash}`
function getDummyUpdateDirectory() {
  const testName = "test_ProfileCounter";
  let dir = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  dir.initWithPath(`C:\\foo\\bar\\${testName}`);
  return dir;
}

// We aren't going to bother generating anything looking like a real client ID
// for this. The only real requirements for client ids is that they not repeat
// and that they be strings. So we'll just return an integer as a string and
// increment it when we want a new client id.
var gDummyTelemetryClientId = 0;
function getDummyTelemetryClientId() {
  return gDummyTelemetryClientId.toString();
}
function setNewDummyTelemetryClientId() {
  ++gDummyTelemetryClientId;
}

// Returns null if the (fake) profile count file hasn't been created yet.
function getProfileCount() {
  // Strict equality to ensure distinguish properly between a non-existent
  // file and an empty one.
  if (gFakeProfileCounterFile === null) {
    return null;
  }
  let saveData = JSON.parse(gFakeProfileCounterFile);
  return saveData.profileTelemetryIds.length;
}

// Resets the state to the original state, before the profile count file has
// even been written.
// If resetFile is specified as false, this will reset everything except for the
// file itself. This allows us to sort of pretend that another installation
// wrote the file.
function reset(resetFile = true) {
  if (resetFile) {
    gFakeProfileCounterFile = null;
  }
  gNextReadExceptionReason = null;
  gNextWriteExceptionReason = null;
  setNewDummyTelemetryClientId();
}

function setup() {
  reset();

  BrowserUsageTelemetry.Policy.readProfileCountFile = async path => {
    if (!gProfileCounterFilePath) {
      gProfileCounterFilePath = path;
    } else {
      // We've only got one mock-file variable. Make sure we are always
      // accessing the same file or this will cause problems.
      Assert.equal(
        gProfileCounterFilePath,
        path,
        "Only one file should be accessed"
      );
    }
    // Strict equality to ensure distinguish properly between null and 0.
    if (gNextReadExceptionReason !== null) {
      let ex = new DOMException(FILE_OPEN_OPERATION, gNextReadExceptionReason);
      gNextReadExceptionReason = null;
      throw ex;
    }
    // Strict equality to ensure distinguish properly between a non-existent
    // file and an empty one.
    if (gFakeProfileCounterFile === null) {
      throw new DOMException(FILE_OPEN_OPERATION, ERROR_FILE_NOT_FOUND);
    }
    return gFakeProfileCounterFile;
  };
  BrowserUsageTelemetry.Policy.writeProfileCountFile = async (path, data) => {
    if (!gProfileCounterFilePath) {
      gProfileCounterFilePath = path;
    } else {
      // We've only got one mock-file variable. Make sure we are always
      // accessing the same file or this will cause problems.
      Assert.equal(
        gProfileCounterFilePath,
        path,
        "Only one file should be accessed"
      );
    }
    // Strict equality to ensure distinguish properly between null and 0.
    if (gNextWriteExceptionReason !== null) {
      let ex = new DOMException(FILE_OPEN_OPERATION, gNextWriteExceptionReason);
      gNextWriteExceptionReason = null;
      throw ex;
    }
    gFakeProfileCounterFile = data;
  };
  BrowserUsageTelemetry.Policy.getUpdateDirectory = getDummyUpdateDirectory;
  BrowserUsageTelemetry.Policy.getTelemetryClientId = getDummyTelemetryClientId;
}

// Checks that the number of profiles reported is the number expected. Because
// of bucketing, the raw count may be different than the reported count.
function checkSuccess(profilesReported, rawCount = profilesReported) {
  Assert.equal(rawCount, getProfileCount());
  const scalars = TelemetryTestUtils.getProcessScalars("parent");
  TelemetryTestUtils.assertScalar(
    scalars,
    PROFILE_COUNT_SCALAR,
    profilesReported,
    "The value reported to telemetry should be the expected profile count"
  );
}

function checkError() {
  const scalars = TelemetryTestUtils.getProcessScalars("parent");
  TelemetryTestUtils.assertScalar(
    scalars,
    PROFILE_COUNT_SCALAR,
    SCALAR_ERROR_VALUE,
    "The value reported to telemetry should be the error value"
  );
}

add_task(async function testProfileCounter() {
  setup();

  info("Testing basic functionality, single install");
  await BrowserUsageTelemetry.reportProfileCount();
  checkSuccess(1);
  await BrowserUsageTelemetry.reportProfileCount();
  checkSuccess(1);

  // Fake another installation by resetting everything except for the profile
  // count file.
  reset(false);

  info("Testing basic functionality, faking a second install");
  await BrowserUsageTelemetry.reportProfileCount();
  checkSuccess(2);

  // Check if we properly handle the case where we cannot read from the file
  // and we have already set its contents. This should report an error.
  info("Testing read error after successful write");
  gNextReadExceptionReason = ERROR_ACCESS_DENIED;
  await BrowserUsageTelemetry.reportProfileCount();
  checkError();

  reset();

  // A read error should cause an error to be reported, but should also write
  // to the file in an attempt to fix it. So the next (successful) read should
  // result in the correct telemetry.
  info("Testing read error self-correction");
  gNextReadExceptionReason = ERROR_ACCESS_DENIED;
  await BrowserUsageTelemetry.reportProfileCount();
  checkError();

  await BrowserUsageTelemetry.reportProfileCount();
  checkSuccess(1);

  reset();

  // If the file is malformed. We should report an error and fix it, then report
  // the correct profile count next time.
  info("Testing with malformed profile count file");
  gFakeProfileCounterFile = "<malformed file data>";
  await BrowserUsageTelemetry.reportProfileCount();
  checkError();

  await BrowserUsageTelemetry.reportProfileCount();
  checkSuccess(1);

  reset();

  // If we haven't yet written to the file, a write error should cause an error
  // to be reported.
  info("Testing write error before the first write");
  gNextWriteExceptionReason = ERROR_ACCESS_DENIED;
  await BrowserUsageTelemetry.reportProfileCount();
  checkError();

  reset();

  info("Testing bucketing");
  // Fake 15 installations to drive the raw profile count up to 15.
  for (let i = 0; i < 15; i++) {
    reset(false);
    await BrowserUsageTelemetry.reportProfileCount();
  }
  // With bucketing, values from 10-99 should all be reported as 10.
  checkSuccess(10, 15);
});
