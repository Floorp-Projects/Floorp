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
});

add_task(async function testAudioDecodingInUtility() {
  await runTest({ expectUtility: true, expectRDD: true });
});

add_task(async function testUtilityTelemetry() {
  const platform = Services.appinfo.OS;
  const extraKey = getExtraKey({ rddPref: true, utilityPref: true });
  for (let exp of utilityPerCodecs[platform]) {
    await verifyTelemetryForProcess(exp.process, exp.codecs, extraKey);
  }
  await verifyNoTelemetryForProcess(
    "rdd",
    ["vorbis", "mp3", "aac", "flac"],
    extraKey
  );
});
