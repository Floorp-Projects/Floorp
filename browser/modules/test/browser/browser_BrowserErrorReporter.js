/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

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

// Wrapper around Services.console.logMessage that waits for the message to be
// logged before resolving, since messages are logged asynchronously.
function logMessage(message) {
  return new Promise(resolve => {
    Services.console.registerListener({
      observe(loggedMessage) {
        if (loggedMessage.message === message.message) {
          Services.console.unregisterListener(this);
          resolve();
        }
      },
    });
    Services.console.logMessage(message);
  });
}

// Clears the console of any previous messages. Should be called at the end of
// each test that logs to the console.
function resetConsole() {
  Services.console.logStringMessage("");
  Services.console.reset();
}

// Wrapper similar to logMessage, but for logStringMessage.
function logStringMessage(message) {
  return new Promise(resolve => {
    Services.console.registerListener({
      observe(loggedMessage) {
        if (loggedMessage.message === message) {
          Services.console.unregisterListener(this);
          resolve();
        }
      },
    });
    Services.console.logStringMessage(message);
  });
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

add_task(async function testInitPrefDisabled() {
  const fetch = sinon.spy();
  const reporter = new BrowserErrorReporter(fetch);
  await SpecialPowers.pushPrefEnv({set: [
    [PREF_ENABLED, false],
    [PREF_SAMPLE_RATE, "1.0"],
  ]});

  reporter.init();
  await logMessage(createScriptError({message: "Logged while disabled"}));
  ok(
    !fetchPassedError(fetch, "Logged while disabled"),
    "Reporter does not listen for errors if the enabled pref is false.",
  );
  reporter.uninit();
  resetConsole();
});

add_task(async function testInitUninitPrefEnabled() {
  const fetch = sinon.spy();
  const reporter = new BrowserErrorReporter(fetch);
  await SpecialPowers.pushPrefEnv({set: [
    [PREF_ENABLED, true],
    [PREF_SAMPLE_RATE, "1.0"],
  ]});

  reporter.init();
  await logMessage(createScriptError({message: "Logged after init"}));
  ok(
    fetchPassedError(fetch, "Logged after init"),
    "Reporter listens for errors if the enabled pref is true.",
  );

  fetch.reset();
  ok(!fetch.called, "Fetch spy was reset.");
  reporter.uninit();
  await logMessage(createScriptError({message: "Logged after uninit"}));
  ok(
    !fetchPassedError(fetch, "Logged after uninit"),
    "Reporter does not listen for errors after uninit.",
  );

  resetConsole();
});

add_task(async function testInitPastMessages() {
  const fetch = sinon.spy();
  const reporter = new BrowserErrorReporter(fetch);
  await SpecialPowers.pushPrefEnv({set: [
    [PREF_ENABLED, true],
    [PREF_SAMPLE_RATE, "1.0"],
  ]});

  await logMessage(createScriptError({message: "Logged before init"}));
  reporter.init();
  ok(
    fetchPassedError(fetch, "Logged before init"),
    "Reporter collects errors logged before initialization.",
  );
  reporter.uninit();
  resetConsole();
});

add_task(async function testEnabledPrefWatcher() {
  const fetch = sinon.spy();
  const reporter = new BrowserErrorReporter(fetch);
  await SpecialPowers.pushPrefEnv({set: [
    [PREF_ENABLED, false],
    [PREF_SAMPLE_RATE, "1.0"],
  ]});

  reporter.init();
  await logMessage(createScriptError({message: "Shouldn't report"}));
  ok(
    !fetchPassedError(fetch, "Shouldn't report"),
    "Reporter does not collect errors if the enable pref is false.",
  );

  await SpecialPowers.pushPrefEnv({set: [
    [PREF_ENABLED, true],
  ]});
  ok(
    !fetchPassedError(fetch, "Shouldn't report"),
    "Reporter does not collect past-logged errors if it is enabled mid-run.",
  );
  await logMessage(createScriptError({message: "Should report"}));
  ok(
    fetchPassedError(fetch, "Should report"),
    "Reporter collects errors logged after the enabled pref is turned on mid-run",
  );

  reporter.uninit();
  resetConsole();
});

add_task(async function testNonErrorLogs() {
  const fetch = sinon.spy();
  const reporter = new BrowserErrorReporter(fetch);
  await SpecialPowers.pushPrefEnv({set: [
    [PREF_ENABLED, true],
    [PREF_SAMPLE_RATE, "1.0"],
  ]});

  reporter.init();

  await logStringMessage("Not a scripterror instance.");
  ok(
    !fetchPassedError(fetch, "Not a scripterror instance."),
    "Reporter does not collect normal log messages or warnings.",
  );

  await logMessage(createScriptError({
    message: "Warning message",
    flags: Ci.nsIScriptError.warningFlag,
  }));
  ok(
    !fetchPassedError(fetch, "Warning message"),
    "Reporter does not collect normal log messages or warnings.",
  );

  await logMessage(createScriptError({
    message: "Non-chrome category",
    category: "totally from a website",
  }));
  ok(
    !fetchPassedError(fetch, "Non-chrome category"),
    "Reporter does not collect normal log messages or warnings.",
  );

  await logMessage(createScriptError({message: "Is error"}));
  ok(
    fetchPassedError(fetch, "Is error"),
    "Reporter collects error messages.",
  );

  reporter.uninit();
  resetConsole();
});

add_task(async function testSampling() {
  const fetch = sinon.spy();
  const reporter = new BrowserErrorReporter(fetch);
  await SpecialPowers.pushPrefEnv({set: [
    [PREF_ENABLED, true],
    [PREF_SAMPLE_RATE, "1.0"],
  ]});

  reporter.init();
  await logMessage(createScriptError({message: "Should log"}));
  ok(
    fetchPassedError(fetch, "Should log"),
    "A 1.0 sample rate will cause the reporter to always collect errors.",
  );

  await SpecialPowers.pushPrefEnv({set: [
    [PREF_SAMPLE_RATE, "0.0"],
  ]});
  await logMessage(createScriptError({message: "Shouldn't log"}));
  ok(
    !fetchPassedError(fetch, "Shouldn't log"),
    "A 0.0 sample rate will cause the reporter to never collect errors.",
  );

  await SpecialPowers.pushPrefEnv({set: [
    [PREF_SAMPLE_RATE, ")fasdf"],
  ]});
  await logMessage(createScriptError({message: "Also shouldn't log"}));
  ok(
    !fetchPassedError(fetch, "Also shouldn't log"),
    "An invalid sample rate will cause the reporter to never collect errors.",
  );

  reporter.uninit();
  resetConsole();
});

add_task(async function testNameMessage() {
  const fetch = sinon.spy();
  const reporter = new BrowserErrorReporter(fetch);
  await SpecialPowers.pushPrefEnv({set: [
    [PREF_ENABLED, true],
    [PREF_SAMPLE_RATE, "1.0"],
  ]});

  reporter.init();
  await logMessage(createScriptError({message: "No name"}));
  let call = fetchCallForMessage(fetch, "No name");
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

  await logMessage(createScriptError({message: "FooError: Has name"}));
  call = fetchCallForMessage(fetch, "Has name");
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

  await logMessage(createScriptError({message: "FooError: Has :extra: colons"}));
  call = fetchCallForMessage(fetch, "Has :extra: colons");
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
  reporter.uninit();
  resetConsole();
});

add_task(async function testFetchArguments() {
  const fetch = sinon.spy();
  const reporter = new BrowserErrorReporter(fetch);
  await SpecialPowers.pushPrefEnv({set: [
    [PREF_ENABLED, true],
    [PREF_SAMPLE_RATE, "1.0"],
    [PREF_PROJECT_ID, "123"],
    [PREF_PUBLIC_KEY, "foobar"],
    [PREF_SUBMIT_URL, "https://errors.example.com/api/123/store/"],
  ]});

  reporter.init();
  const testPageUrl = (
    "chrome://mochitests/content/browser/browser/modules/test/browser/" +
    "browser_BrowserErrorReporter.html"
  );
  const errorRaisedPromise = new Promise(resolve => {
    Services.console.registerListener({
      observe(loggedMessage) {
        if (loggedMessage.message.toString().includes("testFetchArguments error")) {
          Services.console.unregisterListener(this);
          resolve();
        }
      },
    });
  });

  SimpleTest.expectUncaughtException();
  await BrowserTestUtils.withNewTab(testPageUrl, async () => {
    await errorRaisedPromise;

    const call = fetchCallForMessage(fetch, "testFetchArguments error");
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

    Assert.deepEqual(
      body.exception,
      {
        values: [
          {
            type: "Error",
            value: "testFetchArguments error",
            stacktrace: {
              frames: [
                {
                  function: "madeToFail2",
                  filename: testPageUrl,
                  lineno: 15,
                  colno: 15,
                  in_app: true,
                },
                {
                  function: "madeToFail",
                  filename: testPageUrl,
                  lineno: 12,
                  colno: 9,
                  in_app: true,
                },
                {
                  function: null,
                  filename: testPageUrl,
                  lineno: 17,
                  colno: 7,
                  in_app: true,
                }
              ]
            }
          }
        ]
      },
      "Reporter builds stack trace from scriptError correctly.",
    );
  });

  reporter.uninit();
  resetConsole();
});
