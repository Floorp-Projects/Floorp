/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// This file contains BrowserErrorReporter tests that don't depend on
// errors being collected, which is only enabled on Nightly builds.

ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm", this);
ChromeUtils.import("resource:///modules/BrowserErrorReporter.jsm", this);
ChromeUtils.import("resource://gre/modules/AppConstants.jsm", this);
ChromeUtils.import("resource://gre/modules/FileUtils.jsm", this);
ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm", this);

/* global sinon */
Services.scriptloader.loadSubScript(new URL("head_BrowserErrorReporter.js", gTestPath).href, this);

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

add_task(async function testEnabledPrefWatcher() {
  let listening = false;
  const reporter = new BrowserErrorReporter({
    registerListener() {
      listening = true;
    },
    unregisterListener() {
      listening = false;
    },
    now: BrowserErrorReporter.getAppBuildIdDate(),
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

add_task(async function testScalars() {
  // Do not bother testing telemetry scalars if they're already expired.
  if (SCALARS_EXPIRED) {
    return;
  }

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

  const messages = [
    createScriptError({message: "No name"}),
    createScriptError({message: "Also no name", sourceName: "resource://gre/modules/Foo.jsm"}),
    createScriptError({message: "More no name", sourceName: "resource://gre/modules/Bar.jsm"}),
    createScriptError({message: "Yeah sures", sourceName: "unsafe://gre/modules/Bar.jsm"}),
    createScriptError({message: "Addon", sourceName: "moz-extension://foo/Bar.jsm"}),
    createScriptError({
      message: "long",
      sourceName: "resource://gre/modules/long/long/long/long/long/long/long/long/long/long/",
    }),
    {message: "Not a scripterror instance."},
    createScriptError({message: "Whatever", stack: [frame()]}),
  ];

  // Use handleMessage to avoid errors from other code messing up our counts.
  for (const message of messages) {
    await reporter.handleMessage(message);
  }

  const optin = Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN;
  const scalars = Services.telemetry.snapshotScalars(optin, false).parent;
  is(
    scalars[TELEMETRY_ERROR_COLLECTED],
    7,
    `${TELEMETRY_ERROR_COLLECTED} is incremented when an error is collected.`,
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
      "MOZEXTENSION": 1,
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
  // Do not bother testing telemetry scalars if they're already expired.
  if (SCALARS_EXPIRED) {
    return;
  }

  const fetchStub = sinon.stub();
  const reporter = new BrowserErrorReporter({
    fetch: fetchStub,
    now: BrowserErrorReporter.getAppBuildIdDate(),
  });
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

    // Use handleMessage to avoid errors from other code messing up our counts.
    await reporter.handleMessage(createScriptError({
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
