/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// This file contains BrowserErrorReporter tests that depend on errors
// being collected, which is only enabled on Nightly builds.

ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm", this);
ChromeUtils.import("resource:///modules/BrowserErrorReporter.jsm", this);
ChromeUtils.import("resource://gre/modules/AppConstants.jsm", this);
ChromeUtils.import("resource://gre/modules/FileUtils.jsm", this);
ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm", this);

/* global sinon */
Services.scriptloader.loadSubScript(new URL("head_BrowserErrorReporter.js", gTestPath).href, this);

add_task(async function testInitPastMessages() {
  const fetchSpy = sinon.spy();
  const reporter = new BrowserErrorReporter({
    fetch: fetchSpy,
    registerListener: noop,
    unregisterListener: noop,
    now: BrowserErrorReporter.getAppBuildIdDate(),
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

  reporter.uninit();
});

add_task(async function testNonErrorLogs() {
  const fetchSpy = sinon.spy();
  const reporter = new BrowserErrorReporter({
    fetch: fetchSpy,
    now: BrowserErrorReporter.getAppBuildIdDate(),
  });
  await SpecialPowers.pushPrefEnv({set: [
    [PREF_ENABLED, true],
    [PREF_SAMPLE_RATE, "1.0"],
  ]});

  await reporter.handleMessage({message: "Not a scripterror instance."});
  ok(
    !fetchPassedError(fetchSpy, "Not a scripterror instance."),
    "Reporter does not collect normal log messages or warnings.",
  );

  await reporter.handleMessage(createScriptError({
    message: "Warning message",
    flags: Ci.nsIScriptError.warningFlag,
  }));
  ok(
    !fetchPassedError(fetchSpy, "Warning message"),
    "Reporter does not collect normal log messages or warnings.",
  );

  await reporter.handleMessage(createScriptError({
    message: "Non-chrome category",
    category: "totally from a website",
  }));
  ok(
    !fetchPassedError(fetchSpy, "Non-chrome category"),
    "Reporter does not collect normal log messages or warnings.",
  );

  await reporter.handleMessage(createScriptError({message: "Is error"}));
  ok(
    fetchPassedError(fetchSpy, "Is error"),
    "Reporter collects error messages.",
  );
});

add_task(async function testSampling() {
  const fetchSpy = sinon.spy();
  const reporter = new BrowserErrorReporter({
    fetch: fetchSpy,
    now: BrowserErrorReporter.getAppBuildIdDate(),
  });
  await SpecialPowers.pushPrefEnv({set: [
    [PREF_ENABLED, true],
    [PREF_SAMPLE_RATE, "1.0"],
  ]});

  await reporter.handleMessage(createScriptError({message: "Should log"}));
  ok(
    fetchPassedError(fetchSpy, "Should log"),
    "A 1.0 sample rate will cause the reporter to always collect errors.",
  );

  await reporter.handleMessage(createScriptError({message: "undefined", sourceName: undefined}));
  ok(
    fetchPassedError(fetchSpy, "undefined"),
    "A missing sourceName doesn't break reporting.",
  );

  await SpecialPowers.pushPrefEnv({set: [
    [PREF_SAMPLE_RATE, "0.0"],
  ]});
  await reporter.handleMessage(createScriptError({message: "Shouldn't log"}));
  ok(
    !fetchPassedError(fetchSpy, "Shouldn't log"),
    "A 0.0 sample rate will cause the reporter to never collect errors.",
  );

  await reporter.handleMessage(createScriptError({
    message: "chromedevtools",
    sourceName: "chrome://devtools/Foo.jsm",
  }));
  ok(
    fetchPassedError(fetchSpy, "chromedevtools"),
    "chrome://devtools/ paths are sampled at 100% even if the default rate is 0.0.",
  );

  await reporter.handleMessage(createScriptError({
    message: "resourcedevtools",
    sourceName: "resource://devtools/Foo.jsm",
  }));
  ok(
    fetchPassedError(fetchSpy, "resourcedevtools"),
    "resource://devtools/ paths are sampled at 100% even if the default rate is 0.0.",
  );

  await SpecialPowers.pushPrefEnv({set: [
    [PREF_SAMPLE_RATE, ")fasdf"],
  ]});
  await reporter.handleMessage(createScriptError({message: "Also shouldn't log"}));
  ok(
    !fetchPassedError(fetchSpy, "Also shouldn't log"),
    "An invalid sample rate will cause the reporter to never collect errors.",
  );
});

add_task(async function testNameMessage() {
  const fetchSpy = sinon.spy();
  const reporter = new BrowserErrorReporter({
    fetch: fetchSpy,
    now: BrowserErrorReporter.getAppBuildIdDate(),
  });
  await SpecialPowers.pushPrefEnv({set: [
    [PREF_ENABLED, true],
    [PREF_SAMPLE_RATE, "1.0"],
  ]});

  await reporter.handleMessage(createScriptError({message: "No name"}));
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

  await reporter.handleMessage(createScriptError({message: "FooError: Has name"}));
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

  await reporter.handleMessage(createScriptError({message: "FooError: Has :extra: colons"}));
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

add_task(async function testRecentBuild() {
  // Create date that is guaranteed to be a month newer than the build date.
  const nowDate = BrowserErrorReporter.getAppBuildIdDate();
  nowDate.setMonth(nowDate.getMonth() + 1);

  const fetchSpy = sinon.spy();
  const reporter = new BrowserErrorReporter({
    fetch: fetchSpy,
    now: nowDate,
  });
  await SpecialPowers.pushPrefEnv({set: [
    [PREF_ENABLED, true],
    [PREF_SAMPLE_RATE, "1.0"],
  ]});

  await reporter.handleMessage(createScriptError({message: "Is error"}));
  ok(
    !fetchPassedError(fetchSpy, "Is error"),
    "Reporter does not collect errors from builds older than a week.",
  );
});

add_task(async function testFetchArguments() {
  const fetchSpy = sinon.spy();
  const reporter = new BrowserErrorReporter({
    fetch: fetchSpy,
    now: BrowserErrorReporter.getAppBuildIdDate(),
  });
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
    is(
      body.tags.changeset,
      AppConstants.SOURCE_REVISION_URL,
      "Reporter pulls changeset tag from AppConstants",
    );
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
  const reporter = new BrowserErrorReporter({
    fetch: fetchSpy,
    chromeOnly: false,
    now: BrowserErrorReporter.getAppBuildIdDate(),
  });
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
  const reporter = new BrowserErrorReporter({
    fetch: fetchSpy,
    chromeOnly: false,
    now: BrowserErrorReporter.getAppBuildIdDate(),
  });
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

  await reporter.handleMessage(createScriptError({message: "testExtensionTag not from extension"}));
  call = fetchCallForMessage(fetchSpy, "testExtensionTag not from extension");
  body = JSON.parse(call.args[1].body);
  is(body.tags.isExtensionError, false, "Normal errors have an isExtensionError=false tag.");
});

add_task(async function testScalars() {
  const fetchStub = sinon.stub();
  const reporter = new BrowserErrorReporter({
    fetch: fetchStub,
    now: BrowserErrorReporter.getAppBuildIdDate(),
  });
  await SpecialPowers.pushPrefEnv({set: [
    [PREF_ENABLED, true],
    [PREF_SAMPLE_RATE, "1.0"],
  ]});

  Services.telemetry.clearScalars();

  // Basic count
  await reporter.handleMessage(createScriptError({message: "No name"}));

  // Sample rate affects counts
  await SpecialPowers.pushPrefEnv({set: [[PREF_SAMPLE_RATE, "0.0"]]});
  await reporter.handleMessage(createScriptError({message: "Additionally no name"}));

  // Failed fetches should be counted too
  await SpecialPowers.pushPrefEnv({set: [[PREF_SAMPLE_RATE, "1.0"]]});
  fetchStub.rejects(new Error("Could not report"));
  await reporter.handleMessage(createScriptError({message: "Maybe name?"}));

  const optin = Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN;
  const scalars = Services.telemetry.snapshotScalars(optin, false).parent;
  is(
    scalars[TELEMETRY_ERROR_COLLECTED],
    3,
    `${TELEMETRY_ERROR_COLLECTED} is incremented when an error is collected.`,
  );
  is(
    scalars[TELEMETRY_ERROR_SAMPLE_RATE],
    "1.0",
    `${TELEMETRY_ERROR_SAMPLE_RATE} contains the last sample rate used.`,
  );
  is(
    scalars[TELEMETRY_ERROR_REPORTED],
    1,
    `${TELEMETRY_ERROR_REPORTED} is incremented when an error is reported.`,
  );
  is(
    scalars[TELEMETRY_ERROR_REPORTED_FAIL],
    1,
    `${TELEMETRY_ERROR_REPORTED_FAIL} is incremented when an error fails to be reported.`,
  );

  resetConsole();
});

add_task(async function testFilePathMangle() {
  const fetchSpy = sinon.spy();
  const reporter = new BrowserErrorReporter({fetch: fetchSpy});
  await SpecialPowers.pushPrefEnv({set: [
    [PREF_ENABLED, true],
    [PREF_SAMPLE_RATE, "1.0"],
  ]});

  const greDir = Services.dirsvc.get("GreD", Ci.nsIFile).path;
  const profileDir = Services.dirsvc.get("ProfD", Ci.nsIFile).path;

  const message = createScriptError({
    message: "Whatever",
    sourceName: "file:///path/to/main.jsm",
    stack: [
      frame({source: "jar:file:///path/to/jar!/inside/jar.jsm"}),
      frame({source: `file://${greDir}/defaults/prefs/channel-prefs.js`}),
      frame({source: `file://${profileDir}/prefs.js`}),
    ],
  });
  await reporter.handleMessage(message);

  const call = fetchCallForMessage(fetchSpy, "Whatever");
  const body = JSON.parse(call.args[1].body);
  const exception = body.exception.values[0];
  is(exception.module, "[UNKNOWN_LOCAL_FILEPATH]", "Unrecognized local file paths are mangled");

  // Stackframe order is reversed from what is in the message.
  const stackFrames = exception.stacktrace.frames;
  is(
    stackFrames[0].module, "[profileDir]/prefs.js",
    "Paths within the profile directory are preserved but mangled",
  );
  is(
    stackFrames[1].module, "[greDir]/defaults/prefs/channel-prefs.js",
    "Paths within the GRE directory are preserved but mangled",
  );
  is(
    stackFrames[2].module, "/inside/jar.jsm",
    "Paths within jarfiles are extracted from the full jar: URL",
  );
});

add_task(async function testFilePathMangleWhitespace() {
  const fetchSpy = sinon.spy();

  const greDir = Services.dirsvc.get("GreD", Ci.nsIFile);
  const whitespaceDir = greDir.clone();
  whitespaceDir.append("with whitespace");
  const manglePrefixes = {
    whitespace: whitespaceDir,
  };

  const reporter = new BrowserErrorReporter({fetch: fetchSpy, manglePrefixes});
  await SpecialPowers.pushPrefEnv({set: [
    [PREF_ENABLED, true],
    [PREF_SAMPLE_RATE, "1.0"],
  ]});

  const message = createScriptError({
    message: "Whatever",
    sourceName: `file://${greDir.path}/with whitespace/remaining/file.jsm`,
  });
  await reporter.handleMessage(message);

  const call = fetchCallForMessage(fetchSpy, "Whatever");
  const body = JSON.parse(call.args[1].body);
  const exception = body.exception.values[0];
  is(
    exception.module, "[whitespace]/remaining/file.jsm",
    "Prefixes with whitespace are correctly mangled",
  );
});
