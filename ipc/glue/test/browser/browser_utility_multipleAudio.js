/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from head-multiple.js */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/ipc/glue/test/browser/head-multiple.js",
  this
);

add_setup(async function checkAudioDecodingNonUtility() {
  const isAudioDecodingNonUtilityAllowed = await SpecialPowers.getBoolPref(
    "media.allow-audio-non-utility"
  );
  if (isNightlyOrEalyBeta()) {
    ok(
      !isAudioDecodingNonUtilityAllowed,
      "Audio decoding should not be allowed on non utility processes by default on Nightly"
    );
  } else {
    ok(
      isAudioDecodingNonUtilityAllowed,
      "Audio decoding is allowed on non utility processes by default on Beta or Release"
    );
  }
});

add_task(async function testAudioDecodingInUtility() {
  await runTest({ expectUtility: true });
});

add_task(async function testFailureAudioDecodingInRDD() {
  await runTest({ expectUtility: false, expectError: true });
});

add_task(async function testFailureAudioDecodingInContent() {
  // TODO: When getting rid of audio decoding on non utility at all, this
  // should be removed
  if (!isNightlyOrEalyBeta()) {
    return;
  }

  const platform = Services.appinfo.OS;
  if (platform === "WINNT") {
    ok(
      true,
      "Manually skippig on Windows because of gfx killing us, cf browser.ini"
    );
    return;
  }

  await SpecialPowers.pushPrefEnv({
    set: [["media.rdd-process.enabled", false]],
  });
  await runTest({ expectUtility: false, expectRDD: false, expectError: true });
});
