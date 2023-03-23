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

add_task(async function testAudioDecodingInRDD() {
  await runTest({ expectUtility: false, expectRDD: true });
});

add_task(async function testRDDTelemetry() {
  const codecs = ["vorbis", "mp3", "aac", "flac"];
  const extraKey = ",utility-disabled";
  await verifyNoTelemetryForProcess("utility", codecs, extraKey);
  await verifyTelemetryForProcess("rdd", codecs, extraKey);
});
