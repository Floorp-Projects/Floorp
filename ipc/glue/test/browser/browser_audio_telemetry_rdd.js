/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from head-telemetry.js */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/ipc/glue/test/browser/head-telemetry.js",
  this
);

add_setup(async function testNoTelemetry() {
  await Telemetry.clearScalars();
  await SpecialPowers.pushPrefEnv({
    set: [["media.allow-audio-non-utility", true]],
  });
});

add_task(async function testAudioDecodingInRDD() {
  await runTest({ expectUtility: false, expectRDD: true });
});

add_task(async function testRDDTelemetry() {
  const extraKey = getExtraKey({
    rddPref: true,
    utilityPref: false,
    allowNonUtility: true,
  });
  const platform = Services.appinfo.OS;
  for (let exp of utilityPerCodecs[platform]) {
    await verifyNoTelemetryForProcess(exp.process, exp.codecs, extraKey);
  }
  const codecs = ["vorbis", "mp3", "aac", "flac"];
  await verifyTelemetryForProcess("rdd", codecs, extraKey);
});
