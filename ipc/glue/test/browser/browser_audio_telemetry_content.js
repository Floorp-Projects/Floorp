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

add_task(async function testAudioDecodingInContent() {
  await runTest({ expectUtility: false, expectRDD: false });
});

add_task(async function testUtilityTelemetry() {
  const codecs = ["vorbis", "mp3", "aac", "flac"];
  const extraKey = ",rdd-disabled,utility-disabled";
  await verifyTelemetryForProcess("tab", codecs, extraKey);
  await verifyNoTelemetryForProcess("utility", codecs, extraKey);
  await verifyNoTelemetryForProcess("rdd", codecs, extraKey);
});
