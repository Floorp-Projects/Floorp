/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from head-multiple.js */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/ipc/glue/test/browser/head-multiple.js",
  this
);

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["media.utility-process.enabled", false]],
  });
});

add_task(async function testAudioDecodingInUtility() {
  // TODO: When getting rid of audio decoding on non utility at all, this
  // should be removed
  // We only lock the preference in Nightly builds so far, but on beta we expect
  // audio decoding error
  await runTest({ expectUtility: isNightlyOnly(), expectError: isBetaOnly() });
});
