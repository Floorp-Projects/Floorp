/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm", this);
ChromeUtils.import("resource:///modules/BrowserErrorReporter.jsm", this);

/* global sinon */
Services.scriptloader.loadSubScript("resource://testing-common/sinon-2.3.2.js");
registerCleanupFunction(function() {
  delete window.sinon;
});

const PREF_ENABLED = "browser.chrome.errorReporter.enabled";
const PREF_PROJECT_ID = "browser.chrome.errorReporter.projectId";
const PREF_PUBLIC_KEY = "browser.chrome.errorReporter.publicKey";
const PREF_SAMPLE_RATE = "browser.chrome.errorReporter.sampleRate";
const PREF_SUBMIT_URL = "browser.chrome.errorReporter.submitUrl";
const TELEMETRY_ERROR_COLLECTED = "browser.errors.collected_count";
const TELEMETRY_ERROR_COLLECTED_FILENAME = "browser.errors.collected_count_by_filename";
const TELEMETRY_ERROR_COLLECTED_STACK = "browser.errors.collected_with_stack_count";
const TELEMETRY_ERROR_REPORTED = "browser.errors.reported_success_count";
const TELEMETRY_ERROR_REPORTED_FAIL = "browser.errors.reported_failure_count";
const TELEMETRY_ERROR_SAMPLE_RATE = "browser.errors.sample_rate";

function createScriptError(options = {}) {
  const scriptError = Cc["@mozilla.org/scripterror;1"].createInstance(Ci.nsIScriptError);
  scriptError.init(
    options.message || "",
    options.sourceName || null,
    options.sourceLine || null,
    options.lineNumber || null,
    options.columnNumber || null,
    options.flags || Ci.nsIScriptError.errorFlag,
    options.category || "chrome javascript",
  );
  return scriptError;
}

function noop() {
  // Do nothing
}

// Clears the console of any previous messages. Should be called at the end of
// each test that logs to the console.
function resetConsole() {
  Services.console.logStringMessage("");
  Services.console.reset();
}

// Finds the fetch spy call for an error with a matching message.
function fetchCallForMessage(fetchSpy, message) {
  for (const call of fetchSpy.getCalls()) {
    const body = JSON.parse(call.args[1].body);
    if (body.exception.values[0].value.includes(message)) {
      return call;
    }
  }

  return null;
}

// Helper to test if a fetch spy was called with the given error message.
// Used in tests where unrelated JS errors from other code are logged.
function fetchPassedError(fetchSpy, message) {
  return fetchCallForMessage(fetchSpy, message) !== null;
}

add_task(async function testSetup() {
  const canRecordExtended = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;
  registerCleanupFunction(() => Services.telemetry.canRecordExtended = canRecordExtended);
});

add_task(async function testInitPrefDisabled() {
  let listening = false;
  const reporter = new BrowserErrorReporter({
    registerListener() {
      listening = true;
    },
  });
  await SpecialPowers.pushPrefEnv({set: [
    [PREF_ENABLED, false],
  ]});

  reporter.init();
  ok(!listening, "Reporter does not listen for errors if the enabled pref is false.");
});

add_task(async function testInitUninitPrefEnabled() {
  let listening = false;
  const reporter = new BrowserErrorReporter({
    registerListener() {
      listening = true;
    },
    unregisterListener() {
      listening = false;
    },
  });
  await SpecialPowers.pushPrefEnv({set: [
    [PREF_ENABLED, true],
  ]});

  reporter.init();
  ok(listening, "Reporter listens for errors if the enabled pref is true.");

  reporter.uninit();
  ok(!listening, "Reporter does not listen for errors after uninit.");
});

add_task(async function testInitPastMessages() {
  const fetchSpy = sinon.spy();
  const reporter = new BrowserErrorReporter({
    fetch: fetchSpy,
    registerListener: noop,
    unregisterListener: noop,
  });
  await SpecialPowers.pushPrefEnv({set: [
    [PREF_ENABLED, true],
    [PREF_SAMPLE_RATE, "1.0"],
  ]});

  resetConsole();
  Services.console.logMessage(createScriptError({message: "Logged before init"}));
  reporter.init();

  // Include ok() to satisfy mochitest warning for test without any assertions
  const errorWasLogged = await TestUtils.waitForCondition(
    () => fetchPassedError(fetchSpy, "Logged before init"),
    "Waiting for message to be logged",
  );
  ok(errorWasLogged, "Reporter collects errors logged before initialization.");

});

add_task(async function testEnabledPrefWatcher() {
  let listening = false;
  const reporter = new BrowserErrorReporter({
    registerListener() {
      listening = true;
    },
    unregisterListener() {
      listening = false;
    },
  });
  await SpecialPowers.pushPrefEnv({set: [
    [PREF_ENABLED, false],
  ]});

  reporter.init();
  ok(!listening, "Reporter does not collect errors if the enable pref is false.");

  Services.console.logMessage(createScriptError({message: "Shouldn't report"}));
  await SpecialPowers.pushPrefEnv({set: [
    [PREF_ENABLED, true],
  ]});
  ok(listening, "Reporter collects errors if the enabled pref switches to true.");
});

add_task(async function testNonErrorLogs() {
  const fetchSpy = sinon.spy();
  const reporter = new BrowserErrorReporter({fetch: fetchSpy});
  await SpecialPowers.pushPrefEnv({set: [
    [PREF_ENABLED, true],
    [PREF_SAMPLE_RATE, "1.0"],
  ]});

  reporter.observe({message: "Not a scripterror instance."});
  ok(
    !fetchPassedError(fetchSpy, "Not a scripterror instance."),
    "Reporter does not collect normal log messages or warnings.",
  );

  await reporter.observe(createScriptError({
    message: "Warning message",
    flags: Ci.nsIScriptError.warningFlag,
  }));
  ok(
    !fetchPassedError(fetchSpy, "Warning message"),
    "Reporter does not collect normal log messages or warnings.",
  );

  await reporter.observe(createScriptError({
    message: "Non-chrome category",
    category: "totally from a website",
  }));
  ok(
    !fetchPassedError(fetchSpy, "Non-chrome category"),
    "Reporter does not collect normal log messages or warnings.",
  );

  await reporter.observe(createScriptError({message: "Is error"}));
  ok(
    fetchPassedError(fetchSpy, "Is error"),
    "Reporter collects error messages.",
  );
});

add_task(async function testSampling() {
  const fetchSpy = sinon.spy();
  const reporter = new BrowserErrorReporter({fetch: fetchSpy});
  await SpecialPowers.pushPrefEnv({set: [
    [PREF_ENABLED, true],
    [PREF_SAMPLE_RATE, "1.0"],
  ]});

  await reporter.observe(createScriptError({message: "Should log"}));
  ok(
    fetchPassedError(fetchSpy, "Should log"),
    "A 1.0 sample rate will cause the reporter to always collect errors.",
  );

  await SpecialPowers.pushPrefEnv({set: [
    [PREF_SAMPLE_RATE, "0.0"],
  ]});
  await reporter.observe(createScriptError({message: "Shouldn't log"}));
  ok(
    !fetchPassedError(fetchSpy, "Shouldn't log"),
    "A 0.0 sample rate will cause the reporter to never collect errors.",
  );

  await SpecialPowers.pushPrefEnv({set: [
    [PREF_SAMPLE_RATE, ")fasdf"],
  ]});
  await reporter.observe(createScriptError({message: "Also shouldn't log"}));
  ok(
    !fetchPassedError(fetchSpy, "Also shouldn't log"),
    "An invalid sample rate will cause the reporter to never collect errors.",
  );
});

add_task(async function testNameMessage() {
  const fetchSpy = sinon.spy();
  const reporter = new BrowserErrorReporter({fetch: fetchSpy});
  await SpecialPowers.pushPrefEnv({set: [
    [PREF_ENABLED, true],
    [PREF_SAMPLE_RATE, "1.0"],
  ]});

  await reporter.observe(createScriptError({message: "No name"}));
  let call = fetchCallForMessage(fetchSpy, "No name");
  let body = JSON.parse(call.args[1].body);
  is(
    body.exception.values[0].type,
    "Error",
    "Reporter uses a generic type when no name is in the message.",
  );
  is(
    body.exception.values[0].value,
    "No name",
    "Reporter uses error message as the exception value.",
  );

  await reporter.observe(createScriptError({message: "FooError: Has name"}));
  call = fetchCallForMessage(fetchSpy, "Has name");
  body = JSON.parse(call.args[1].body);
  is(
    body.exception.values[0].type,
    "FooError",
    "Reporter uses the error type from the message.",
  );
  is(
    body.exception.values[0].value,
    "Has name",
    "Reporter uses error message as the value parameter.",
  );

  await reporter.observe(createScriptError({message: "FooError: Has :extra: colons"}));
  call = fetchCallForMessage(fetchSpy, "Has :extra: colons");
  body = JSON.parse(call.args[1].body);
  is(
    body.exception.values[0].type,
    "FooError",
    "Reporter uses the error type from the message.",
  );
  is(
    body.exception.values[0].value,
    "Has :extra: colons",
    "Reporter uses error message as the value parameter.",
  );
});

add_task(async function testFetchArguments() {
  const fetchSpy = sinon.spy();
  const reporter = new BrowserErrorReporter({fetch: fetchSpy});
  await SpecialPowers.pushPrefEnv({set: [
    [PREF_ENABLED, true],
    [PREF_SAMPLE_RATE, "1.0"],
    [PREF_PROJECT_ID, "123"],
    [PREF_PUBLIC_KEY, "foobar"],
    [PREF_SUBMIT_URL, "https://errors.example.com/api/123/store/"],
  ]});

  resetConsole();
  reporter.init();
  const testPageUrl = (
    "chrome://mochitests/content/browser/browser/modules/test/browser/" +
    "browser_BrowserErrorReporter.html"
  );

  SimpleTest.expectUncaughtException();
  await BrowserTestUtils.withNewTab(testPageUrl, async () => {
    const call = await TestUtils.waitForCondition(
      () => fetchCallForMessage(fetchSpy, "testFetchArguments error"),
      "Wait for error from browser_BrowserErrorReporter.html to be logged",
    );
    const body = JSON.parse(call.args[1].body);
    const url = new URL(call.args[0]);

    is(url.origin, "https://errors.example.com", "Reporter builds API url from DSN pref.");
    is(url.pathname, "/api/123/store/", "Reporter builds API url from DSN pref.");
    is(
      url.searchParams.get("sentry_client"),
      "firefox-error-reporter/1.0.0",
      "Reporter identifies itself in the outgoing request",
    );
    is(url.searchParams.get("sentry_version"), "7", "Reporter is compatible with Sentry 7.");
    is(url.searchParams.get("sentry_key"), "foobar", "Reporter pulls API key from DSN pref.");
    is(body.project, "123", "Reporter pulls project ID from DSN pref.");
    is(call.args[1].referrer, "https://fake.mozilla.org", "Reporter uses a fake referer.");

    const response = await fetch(testPageUrl);
    const pageText = await response.text();
    const pageLines = pageText.split("\n");
    Assert.deepEqual(
      body.exception,
      {
        values: [
          {
            type: "Error",
            value: "testFetchArguments error",
            module: testPageUrl,
            stacktrace: {
              frames: [
                {
                  function: null,
                  module: testPageUrl,
                  lineno: 17,
                  colno: 7,
                  pre_context: pageLines.slice(11, 16),
                  context_line: pageLines[16],
                  post_context: pageLines.slice(17, 22),
                },
                {
                  function: "madeToFail",
                  module: testPageUrl,
                  lineno: 12,
                  colno: 9,
                  pre_context: pageLines.slice(6, 11),
                  context_line: pageLines[11],
                  post_context: pageLines.slice(12, 17),
                },
                {
                  function: "madeToFail2",
                  module: testPageUrl,
                  lineno: 15,
                  colno: 15,
                  pre_context: pageLines.slice(9, 14),
                  context_line: pageLines[14],
                  post_context: pageLines.slice(15, 20),
                },
              ],
            },
          },
        ],
      },
      "Reporter builds stack trace from scriptError correctly.",
    );
  });

  reporter.uninit();
});

add_task(async function testAddonIDMangle() {
  const fetchSpy = sinon.spy();
  // Passing false here disables category checks on errors, which would
  // otherwise block errors directly from extensions.
  const reporter = new BrowserErrorReporter({fetch: fetchSpy, chromeOnly: false});
  await SpecialPowers.pushPrefEnv({set: [
    [PREF_ENABLED, true],
    [PREF_SAMPLE_RATE, "1.0"],
  ]});
  resetConsole();
  reporter.init();

  // Create and install test add-on
  const id = "browsererrorcollection@example.com";
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {
        gecko: { id },
      },
    },
    background() {
      throw new Error("testAddonIDMangle error");
    },
  });
  await extension.startup();

  // Just in case the error hasn't been thrown before add-on startup.
  const call = await TestUtils.waitForCondition(
    () => fetchCallForMessage(fetchSpy, "testAddonIDMangle error"),
    `Wait for error from ${id} to be logged`,
  );
  const body = JSON.parse(call.args[1].body);
  const stackFrame = body.exception.values[0].stacktrace.frames[0];
  ok(
    stackFrame.module.startsWith(`moz-extension://${id}/`),
    "Stack frame filenames use the proper add-on ID instead of internal UUIDs.",
  );

  await extension.unload();
  reporter.uninit();
});

add_task(async function testExtensionTag() {
  const fetchSpy = sinon.spy();
  // Passing false here disables category checks on errors, which would
  // otherwise block errors directly from extensions.
  const reporter = new BrowserErrorReporter({fetch: fetchSpy, chromeOnly: false});
  await SpecialPowers.pushPrefEnv({set: [
    [PREF_ENABLED, true],
    [PREF_SAMPLE_RATE, "1.0"],
  ]});
  resetConsole();
  reporter.init();

  // Create and install test add-on
  const id = "browsererrorcollection@example.com";
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {
        gecko: { id },
      },
    },
    background() {
      throw new Error("testExtensionTag error");
    },
  });
  await extension.startup();

  // Just in case the error hasn't been thrown before add-on startup.
  let call = await TestUtils.waitForCondition(
    () => fetchCallForMessage(fetchSpy, "testExtensionTag error"),
    `Wait for error from ${id} to be logged`,
  );
  let body = JSON.parse(call.args[1].body);
  ok(body.tags.isExtensionError, "Errors from extensions have an isExtensionError=true tag.");

  await extension.unload();
  reporter.uninit();

  await reporter.observe(createScriptError({message: "testExtensionTag not from extension"}));
  call = fetchCallForMessage(fetchSpy, "testExtensionTag not from extension");
  body = JSON.parse(call.args[1].body);
  is(body.tags.isExtensionError, false, "Normal errors have an isExtensionError=false tag.");
});

add_task(async function testScalars() {
  const fetchStub = sinon.stub();
  const reporter = new BrowserErrorReporter({fetch: fetchStub});
  await SpecialPowers.pushPrefEnv({set: [
    [PREF_ENABLED, true],
    [PREF_SAMPLE_RATE, "1.0"],
  ]});

  Services.telemetry.clearScalars();

  const messages = [
    createScriptError({message: "No name"}),
    createScriptError({message: "Also no name", sourceName: "resource://gre/modules/Foo.jsm"}),
    createScriptError({message: "More no name", sourceName: "resource://gre/modules/Bar.jsm"}),
    createScriptError({message: "Yeah sures", sourceName: "unsafe://gre/modules/Bar.jsm"}),
    createScriptError({
      message: "long",
      sourceName: "resource://gre/modules/long/long/long/long/long/long/long/long/long/long/",
    }),
    {message: "Not a scripterror instance."},

    // No easy way to create an nsIScriptError with a stack, so let's pretend.
    Object.create(
      createScriptError({message: "Whatever"}),
      {stack: {value: new Error().stack}},
    ),
  ];

  // Use observe to avoid errors from other code messing up our counts.
  for (const message of messages) {
    await reporter.observe(message);
  }

  await SpecialPowers.pushPrefEnv({set: [[PREF_SAMPLE_RATE, "0.0"]]});
  await reporter.observe(createScriptError({message: "Additionally no name"}));

  await SpecialPowers.pushPrefEnv({set: [[PREF_SAMPLE_RATE, "1.0"]]});
  fetchStub.rejects(new Error("Could not report"));
  await reporter.observe(createScriptError({message: "Maybe name?"}));

  const optin = Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN;
  const scalars = Services.telemetry.snapshotScalars(optin, false).parent;
  is(
    scalars[TELEMETRY_ERROR_COLLECTED],
    8,
    `${TELEMETRY_ERROR_COLLECTED} is incremented when an error is collected.`,
  );
  is(
    scalars[TELEMETRY_ERROR_SAMPLE_RATE],
    "1.0",
    `${TELEMETRY_ERROR_SAMPLE_RATE} contains the last sample rate used.`,
  );
  is(
    scalars[TELEMETRY_ERROR_REPORTED],
    6,
    `${TELEMETRY_ERROR_REPORTED} is incremented when an error is reported.`,
  );
  is(
    scalars[TELEMETRY_ERROR_REPORTED_FAIL],
    1,
    `${TELEMETRY_ERROR_REPORTED_FAIL} is incremented when an error fails to be reported.`,
  );
  is(
    scalars[TELEMETRY_ERROR_COLLECTED_STACK],
    1,
    `${TELEMETRY_ERROR_REPORTED_FAIL} is incremented when an error with a stack trace is collected.`,
  );

  const keyedScalars = Services.telemetry.snapshotKeyedScalars(optin, false).parent;
  Assert.deepEqual(
    keyedScalars[TELEMETRY_ERROR_COLLECTED_FILENAME],
    {
      "FILTERED": 1,
      "resource://gre/modules/Foo.jsm": 1,
      "resource://gre/modules/Bar.jsm": 1,
      // Cut off at 70-character limit
      "resource://gre/modules/long/long/long/long/long/long/long/long/long/l": 1,
    },
    `${TELEMETRY_ERROR_COLLECTED_FILENAME} is incremented when an error is collected.`,
  );

  resetConsole();
});

add_task(async function testCollectedFilenameScalar() {
  const fetchStub = sinon.stub();
  const reporter = new BrowserErrorReporter(fetchStub);
  await SpecialPowers.pushPrefEnv({set: [
    [PREF_ENABLED, true],
    [PREF_SAMPLE_RATE, "1.0"],
  ]});

  const testCases = [
    ["chrome://unknown/module.jsm", false],
    ["resource://unknown/module.jsm", false],
    ["unknown://unknown/module.jsm", false],

    ["resource://gre/modules/Foo.jsm", true],
    ["resource:///modules/Foo.jsm", true],
    ["chrome://global/Foo.jsm", true],
    ["chrome://browser/Foo.jsm", true],
    ["chrome://devtools/Foo.jsm", true],
  ];

  for (const [filename, shouldMatch] of testCases) {
    Services.telemetry.clearScalars();

    // Use observe to avoid errors from other code messing up our counts.
    await reporter.observe(createScriptError({
      message: "Fine",
      sourceName: filename,
    }));

    const keyedScalars = (
      Services.telemetry.snapshotKeyedScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false).parent
    );

    let matched = null;
    if (shouldMatch) {
      matched = keyedScalars[TELEMETRY_ERROR_COLLECTED_FILENAME][filename] === 1;
    } else {
      matched = keyedScalars[TELEMETRY_ERROR_COLLECTED_FILENAME].FILTERED === 1;
    }

    ok(
      matched,
      shouldMatch
        ? `${TELEMETRY_ERROR_COLLECTED_FILENAME} logs a key for ${filename}.`
        : `${TELEMETRY_ERROR_COLLECTED_FILENAME} logs a FILTERED key for ${filename}.`,
    );
  }

  resetConsole();
});
