/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);

const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

// This is a list of all extra keys that are exposed to telemetry. Please only
// add things to this list with great care and proper code review from relevant
// module owner/peers and proper data review from data stewards.
const allowedExtraKeys = [
  "context",
  "frame_id",
  "process_id",
  "result",
  "seq",
  "severity",
  "source_file",
  "source_line",
  "stack_id",
];

const originSchemes = [
  "http",
  "https",
  "ftp",
  "ws",
  "wss",
  "gopher",
  "blob",
  "file",
  "data",
];

const testcases = [
  // Test temporary storage initialization with and without a broken origin
  // directory.
  {
    async setup(expectedInitResult) {
      Services.prefs.setBoolPref("dom.quotaManager.loadQuotaFromCache", false);

      let request = init();
      await requestFinished(request);

      request = initTemporaryStorage();
      await requestFinished(request);

      request = initTemporaryOrigin(
        "default",
        getPrincipal("https://example.com")
      );
      await requestFinished(request);

      const usageFile = getRelativeFile(
        "storage/default/https+++example.com/ls/usage"
      );

      if (!expectedInitResult) {
        usageFile.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
      } else {
        usageFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);
      }

      // XXX It would be nice to have a method which shuts down temporary
      // storage only. Now we have to shut down entire storage (including
      // temporary storage) and then initialize storage again.
      request = reset();
      await requestFinished(request);

      request = init();
      await requestFinished(request);
    },
    initFunction: initTemporaryStorage,
    getExpectedNumberOfEvents() {
      if (AppConstants.EARLY_BETA_OR_EARLIER || AppConstants.DEBUG) {
        if (AppConstants.NIGHTLY_BUILD) {
          return {
            initFailure: 9,
            initSuccess: 0,
          };
        }

        return {
          initFailure: 14,
          initSuccess: 0,
        };
      }

      return {
        initFailure: 0,
        initSuccess: 0,
      };
    },
    async cleanup() {
      const request = clear();
      await requestFinished(request);

      Services.prefs.setBoolPref("dom.quotaManager.loadQuotaFromCache", true);
    },
  },
];

function verifyEvents(expectedNumberOfEvents) {
  const events = TelemetryTestUtils.getEvents({
    category: "dom.quota.try",
    method: "error",
  });

  is(
    events.length,
    expectedNumberOfEvents,
    "The number of events must match the expected number of events"
  );

  for (const event of events) {
    for (const extraKey in event.extra) {
      ok(
        allowedExtraKeys.includes(extraKey),
        `The extra key ${extraKey} must be in the allow list`
      );

      const extraValue = event.extra[extraKey];

      // These are extra paranoia checks to ensure extra values don't contain
      // origin like strings.
      for (const suffix of ["://", "+++"]) {
        ok(
          originSchemes.every(
            originScheme => !extraValue.includes(originScheme + suffix)
          ),
          `The extra value ${extraValue} must not contain origin like strings`
        );
      }
    }
  }
}

async function testSteps() {
  for (const testcase of testcases) {
    for (const expectedInitResult of [false, true]) {
      // Clear all events.
      Services.telemetry.clearEvents();

      info(
        `Verifying the events when the initialization ` +
          `${expectedInitResult ? "succeeds" : "fails"}`
      );

      await testcase.setup(expectedInitResult);

      const msg = `Should ${expectedInitResult ? "not " : ""} have thrown`;

      let request = testcase.initFunction();
      try {
        await requestFinished(request);
        ok(expectedInitResult, msg);
      } catch (ex) {
        ok(!expectedInitResult, msg);
      }

      const expectedNumberOfEventsObject = testcase.getExpectedNumberOfEvents
        ? testcase.getExpectedNumberOfEvents()
        : testcase.expectedNumberOfEvents;

      const expectedNumberOfEvents = expectedInitResult
        ? expectedNumberOfEventsObject.initSuccess
        : expectedNumberOfEventsObject.initFailure;

      verifyEvents(expectedNumberOfEvents);

      await testcase.cleanup();
    }
  }
}
